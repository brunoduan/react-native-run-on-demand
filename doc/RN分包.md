
## RN分包

App里面有社区、商城、钱包等业务使用RN（React Native），业务bundle压缩之后就达到1.5M，对业务加载性能带来非常大的挑战。传统的优化方案是进行业务预加载，即在用户未真正进入RN页面之前提前加载bundle。该方案虽说可以优化bundle加载性能，但对app启动性能增加负担，app启动过程不够流畅。随着业务复杂度上来之后，bundle只会越来越大，js引擎在解析bundle上面花费的时间随着bundle的增大而陡然上升。另外对于多个业务，每个业务都需要重复的RN框架层代码，预加载方案是无能为力进行去重的。本文要介绍的方案是将bundle拆分成common bundle和几个business bundle，然后分步加载小的bundle。其中，common bundle表示几个business bundle共用的js代码，business bundle里面只用包含跟本业务相关的代码。拆分出多个子bundle之后，可以减轻bundle加载性能压力，为业务按需加载提供可能。

注1：RN分包是有理论基础的，可以翻看js引擎相关benchmark，一般都会提到js文件大小对于js解析编译耗时的影响。js文件大小在现实中的情况，可以查看[Http Archive]([https://httparchive.org/reports/state-of-the-web\#numUrls)。经过统计，发现2018年7月份之前移动端运行的js文件平局大小为460K。同样是js代码，RN的bundle没有道理达到几M的级别。

注2: 本文档提到的RN版本基于0.53.3

## 2. 分包方案选型

首先跟大家一起了解一下RN打包产物的结构，由哪几个部分组成的，方便理解后面的几个分包优化方案。注意，这里谈到的RN分包是一个统称，除了分包，必然包含合包。

### 2.1 认识bundle结构

![](https://4ndroidev.github.io/images/react-native-bundle/bundle-structure.png)

- polyfills：简单理解为一堆全局函数以及变量，包含es6语法新增接口，定义module的函数“function \_\_d\(\)”等等。
- module定义：占比最大的部分，通过`__d`定义了一个个module，每个module定义了文件路径，对外输出的代码，有自己唯一的module id，有其依赖的其他所有module的id，从而在编译阶段就能确定依赖关系。
- require calls：通过require某个module id来加载并运行module里面的代码。

在RN打包过程中，会根据打包入口文件分析出module依赖，然后为每个module生成module定义，并在最后require InitializeCore这个module以及业务入口文件对应的module，整个打包过程大致完成了。

### 2.2 分包方案

一般有以下三种优化方案：

- 方案A：按bundle大小强制分割。
- 方案B：首先利用空工程生成common bundle，然后正常打包生成整个bundle。两个bundle生成之后，利用diff工具生成business bundle。
- 方案C：更改RN打包流程，打包过程中打出common bundle以及business bundle。

方案A比较容易理解，一般将一个大bundle分割成几个相同大小的子bundle，对bundle内部组成完全不用理会，带来的后果就是不能区分common bundle。如果是多个业务，每个业务都会包含重复的common bundle。由于不能理解bundle内部组成，自然无法做到按需加载多个业务bundle，只能在业务启动加载的时候一股脑儿加载其所有子bundle。

方案B利用diff工具将一个大bundle分割成两个bundle：common和business。该方案跟方案A一样，对bundle内部组成完全不用理会，方案A具备的问题它同样具有。另外，由于是通过diff工具diff提取的business bundle，打乱了业务代码的执行顺序，导致通过RN打包工具打出来的sourcemap文件无效。当发生js异常的时候，通过该sourcemap文件是无法解析出原始的js堆栈的。

方案C侵入RN打包工具[metro]([https://github.com/facebook/metro)的打包流程，打包过程中能够理解bundle内部组成，区分哪些module是来自common，哪些来自business，打包出的business bundle的执行顺序不会发生变化。对于多个业务bundle，只需要传入不同的业务入口就能打出对应的business bundle，共用同一个common bundle。

以下表格分别从兼容性、复用性、可维护性、支持异常解析及按需加载这5个维度进行总结：

| 方案 | 兼容ios | 多bundle复用common | 侵入打包流程 | 支持异常解析 | 支持按需加载 |
| ---- | ------- | ------------------ | ------------ | ------------ | ------------ |
| A    | 是      | 否                 | 是           | 是           | 否           |
| B    | 是      | 否                 | 否           | 否           | 否           |
| C    | 是      | 是                 | 是           | 是           | 是           |

### 2.3 合包

确定好分包方案C之后，合包比较简单。即在app端首先运行common bundle，初始化js引擎执行环境（js context）。然后在加载业务bundle之后，只需要将该业务bundle注入到js context。由于common bundle代码已经执行过，那么js context里面包含了运行business bundle所需要的所有可执行上下文，business bundle可以毫无障碍地运行。

## 3. 打包流程优化方案

RN使用[metro]([https://github.com/facebook/metro)生成bundle，跟webpack干的事情一样，但metro可以分析module之间的依赖关系，从而为定制module之间的先后顺序提供了可能。简单起见，我们假设module之间存在一种依赖，即来自business的module依赖于来自common的module，这样将所有module划分为两个阵营：common modules以及business modules，common在前，business在后。

### 3.1 划分module阵营

当时考虑了两种方案，从module的路径入手；或者事先通过某种方式确定好common modules，在打包过程中筛选出business modules。由于业务路径是确定的，故在打包过程中就能够对module阵营进行划分，可以在module中设置一个标识。该方案实现起来比较简单，可以达到分包的目的，但由于不同业务对应的common modules还是有差异的，这样每个业务会生成各自的common modules，不能达到复用common bundle的目的。故本文考虑事先生成common modules的方案。

### 3.2 生成common modules

基本思想：新增一个common.js入口文件，该入口文件里面包含了所有业务引入的依赖，然后利用打包命令进行打包，此时打出的bundle里面只会包含所有common modules。在打包过程当中，会提取每个common module的id以及路径，并序列化到本地文件。默认情况下，module的id的生成算法比较简单，即递增加1。

common.js文件示例：

```
// import react, react-native                                                                                                                                                                                                                                                
import * as R1 from 'react'
import * as R2 from 'react-native'
import * as R3 from 'react-native-action-button'
import * as R4 from 'react-native-background-timer'
import * as R5 from 'react-native-image-crop-picker'
import * as R6 from 'react-native-image-sequence-podspec'

// import business base modules
import * as B1 from '@merryjs/photo-viewer'
import * as B2 from 'js-sha512'
import * as B3 from 'moment'
import * as B4 from 'pegjs'
import * as B5 from 'fbjs'
import * as B6 from 'path-to-regexp'
import * as B7 from 'prop-types'
import * as B8 from 'jsonify'
```

由于以上方式会import某个module里面所有代码，增大bundle体积，可以针对大的module进行定制优化，只import使用到的代码。对于多个业务，它们完全可以共享同一份common bundle。前提是对业务代码进行充分抽象和解耦，提炼出公共的基础业务模块。
以上方案还支持更加复杂的场景。比如针对不同的业务场景，如果多个业务A、B、C等依赖的base modules需要比较晚才用到，那么就没有必要都打到common bundle。我们完全可以另外新建另一个入口文件，该文件只会import这些业务依赖的base modules，然后生成对应的base modules的bundle文件。那么在加载业务A的时候，除了加载common bundle，还会加载base modules的bundle，最后才是加载业务A的module。

### 3.3 生成business bundle

有了common modules对应的序列化文件之后，下一次打business bundle的时候会首先读取该序列化文件。如果某个modue的id不在这个文件里面，那么该module即为business module，此时打出的bundle只会包含business module的代码了。

## 4. react native改造

分为两个部分native端和js端：nativve端增加接口用于加载common和business bundle；js端用于接收打包参数并提供module id的生成算法给metro使用。

### 4.1 native端

App在使用RN的时候，多个业务共享的是同一个RN实例ReactInstanceManager。需要在ReactInstanceManager里面增加以下两个接口：

- createCommonReactContext(): 加载并运行common bundle，创建common bundle可执行上下文
- createBusinessReactContext(): 加载并运行business bundle

```Java
  public void createCommonReactContext() {
    if (!hasStartedCreatingInitialContext()) {
      createReactContextInBackground();
    }
  }

  public void createBusinessReactContext()
          throws Exception {
    CatalystInstance instance = getCurrentReactContext().getCatalystInstance();
    Field field = instance.getClass().getDeclaredField("mJSBundleLoader");
    field.setAccessible(true);
    field.set(instance, mBundleLoader);
    field = instance.getClass().getDeclaredField("mJSBundleHasLoaded");
    field.setAccessible(true);
    field.set(instance, false);
    instance.runJSBundle();
  }
```

通过重载ReactNativeHost里面的方法getJSBundleFile()或者getBundleAssetName()，就可以设置common bundle的文件路径，那么RN内部会去创建对应的JSBundleLoader。对于business bundle，可以在业务入口比如某个Activity的onCreate()里面创建JSBundeLoader并设置到ReactInstanceManager的成员变量mBundleLoader（这里采用反射）。假设从assets加载business bundle：

```Java
  private static void onLoadJSBundleFromAssets(Activity activity,
                                               ReactInstanceManager manager,
                                               ReactBundle bundle) {
        String assetsUrl = 'your business bundle assets path'
        //create a new JSBundleLoader
        JSBundleLoader jsBundleLoader = JSBundleLoader.createAssetLoader(
            activity.getApplicationContext(), assetsUrl, false);
        try {
            // reset JSBundleLoader
            Field field = manager.getClass().getDeclaredField("mBundleLoader");
            field.setAccessible(true);
            field.set(manager, jsBundleLoader);
            // reset current Activity if the Activity has resumed
            Field currentActivity = manager.getClass().getDeclaredField("mCurrentActivity");
            currentActivity.setAccessible(true);
            currentActivity.set(manager, activity);

            manager.createBusinessReactContext();
        } catch (Exception e) {
        }
    }
```

### 4.2 js端

#### 4.2.1 打包参数

打包RN业务的时候，使用如下代码：

```Shell
react-native bundle \
    --entry-file "entry/common.js" \
    --bundle-output "build/output/common.bundle"
    --platform "android"
    --dev false
```

RN的cli脚本接收以上打包参数。为了区分打出common bundle以及business bundle，在local-cli/bundle/bundleCommandLineArgs.js里面提供了两个参数：

```Javascript
  {
    command: '--common-build [boolean]',
    description: 'If true, only build the common bundle',
    parse: (val) => val === 'true' ? true : false,
    default: false,
  },
  {
    command: '--business-build [boolean]',
    description: 'If true and --common-build equals false, only build the common bundle',
    parse: (val) => val === 'true' ? true : false,
    default: false,
  },
```

如果要打common bundle，使用以下命令：

```Shell
react-native bundle \
    --entry-file "entry/common.js" \
    --bundle-output "build/output/common.bundle"
    --platform "android"
    --common-build true
    --business-build false
    --dev false
```

如果要打business bundle，使用以下命令：

```Shell
react-native bundle \
    --entry-file "entry/common.js" \
    --bundle-output "build/output/common.bundle"
    --platform "android"
    --common-build false
    --business-build true
    --dev false
```

#### 4.2.2 module id生成算法

在local-cli/bundle/buildBundle.js里面，除了将以上打包参数传入metro之外，还需要提供新的module id生成算法供metro使用：

```Javascript
  const requestOpts: RequestOptions = {
    entryFile: args.entryFile,
    sourceMapUrl,
    dev: args.dev,
    minify: args.minify !== undefined ? args.minify : !args.dev,
    platform: args.platform,
    /* #ifdef XPENG_BUILD_SPLIT_BUNDLE */
    commonBuild: args.commonBuild,
    businessBuild: args.businessBuild,
    bundleOutput: args.bundleOutput,
    createModuleIdFactory: _createModuleIdFactory,
    /* #endif */
  };
  
  function _createModuleIdFactory() {
    let nextId = 0;
    let filenameIds = path.dirname(bundleOutput);
    let commonIdsArray = [];
    // common id文件
    filenameIds = path.join(filenameIds, "XP_BUILD_SPLIT_BUNDLE_COMMON.ids");
    try {
      const commonStr = fs.readFileSync(filenameIds, 'utf8');
      commonIdsArray = JSON.parse(commonStr) || [];
      nextId = 8080;
    } catch (err) {}
    const commonIds = new Map(commonIdsArray.map(i => [i.path, i.id]));
    const ids = new Map();
    return (path) => {
      let id = ids.get(path);
      if (typeof id !== 'number') {
        id = commonIds.get(path);
        if (typeof id === 'number') {
          ids.set(path, id);
          return id;
        }

        // not found in commonIds
        id = nextId++;
        ids.set(path, id);
      }
      return id;
    };
  };
```

## 5. metro改造

App使用RN的版本为0.55.3，对应的metro版本为0.30.2。对metro进行代码侵入改造，需要考虑维护成本。由于App使用RN的一个策略就是不会频繁进行RN版本升级，故在这块的改造成本可以进行控制。

### 5.1 序列化module ids

新增了一个函数fullModuleIds()用于在打common bundle过程中获取每个module相关的path和id信息，以json数组保存，见packages/metro/src/DeltaBundler/Serializers/Serializers.js：

```Javascript
async function fullModuleIds(
  deltaBundler: DeltaBundler,
  options: BundleOptions,
): Promise<$ReadOnlyArray<{path: string, id: number}>> {
  const {modules} = await _getAllModules(deltaBundler, options);
  //return modules.map(m => {path: m.path, id: m.id});
  return modules.map(function (m) {
    return {
      path: m.path,
      id: m.id
    };
  });
}
```

metro在打包最后会将打包产物写到本地文件，见函数saveBundleAndMap()，位于packages/metro/src/shared/output/bundle.js。序列化module ids也是放在了该函数之内：

```Java
function saveBundleAndMap(...) {
  const commonBuild = options.commonBuild;
  if (commonBuild) {
    const ids = bundle.moduleIds || [];
    if (ids.length > 0) {
      const path = require('path');
      let filenameIds = path.dirname(bundleOutput);
      filenameIds = path.join(filenameIds, "XP_BUILD_SPLIT_BUNDLE_COMMON.ids");
      const writeModuleIds = writeFile(
          filenameIds,
          JSON.stringify(ids),
          encoding).then(() => log('Done writing common bundle output'));
      Promise.all([writeBundle, writeMetadata, writeModuleIds]);
    }
  }
}
```

其中，XP_BUILD_SPLIT_BUNDLE_COMMON.ids文件用于保存module相关的<module path, module id>，目前该文件名是hardcode的，意味着目前只支持打一个common bundle。

### 5.2 使用自定义的module id生成算法

在DeltaTransformer的构造函数里面使用react-native传入的createModuleIdFactory：

```Javascript
this._getModuleId = this._bundleOptions.isolateModuleIDs && !this._bundleOptions.commonBuild && this._bundleOptions.businessBuild
      ? (bundleOptions.createModuleIdFactory || createModuleIdFactory)()
      : globalCreateModuleId;
```

只在打business bundle的时候才需要使用bundleOptions.createModuleIdFactory这个生成算法。

### 5.3 筛选business modules

metro通过函数getAllModues()来获取打包过程中的所有modules，见packages/metro/src/DeltaBundler/DeltaPatcher.js。为了方便获取common或business相关modules，在DeltaPatcher里面新增了一个同名的函数：

```Javascript
  /* #ifdef XPENG_BUILD_SPLIT_BUNDLE */
  getAllModules(
    modifierFn: (
      modules: $ReadOnlyArray<DeltaEntry>,
    ) => $ReadOnlyArray<DeltaEntry> = modules => modules,
    options,
  ): $ReadOnlyArray<DeltaEntry> {
    let modules = [].concat(
      Array.from(this._lastBundle.pre.values()),
      modifierFn(Array.from(this._lastBundle.modules.values())),
      Array.from(this._lastBundle.post.values()),
    );
    let commonBuild = options.commonBuild;
    let businessBuild = options.businessBuild;
    if (businessBuild && !commonBuild) {
      if (DeltaPatcher._commonModuleIds === undefined) {
        let bundleOutput = options.bundleOutput;
        _initializeCommonModuleIds(bundleOutput);
      }
      let businessModules = modules.filter(m =>
          !DeltaPatcher._commonModuleIds.has(m.path) &&
          m.path.indexOf('sourcemap.js') == -1);
      modules = businessModules;
    }
    return modules;
  }
  /* #endif */
}

/* #ifdef XPENG_BUILD_SPLIT_BUNDLE */
const fs = require('fs');
const path = require('path');
function _initializeCommonModuleIds(bundleOutput: string) {
  if (bundleOutput === undefined) {
    return;
  }
  let filenameIds = path.dirname(bundleOutput);
  filenameIds = path.join(filenameIds, "XP_BUILD_SPLIT_BUNDLE_COMMON.ids");
  const commonStr = fs.readFileSync(filenameIds, 'utf8');
  const commonIdsArray = JSON.parse(commonStr) || [];
  DeltaPatcher._commonModuleIds = new Map(commonIdsArray.map(i => [i.path, i.id]));
}
/* #endif */
```

以上代码里面，当businessBuild为true并且commonBuild为false的时候，表示拿的是business相关的modules，此时返回的modules数组里面会删除common modules。

### 5.4 js异常解析

当js代码运行发生异常的情况下，RN会将js端的运行堆栈抛到java端，java端会包装堆栈信息成一个java异常并直接崩溃产生崩溃日志，这样崩溃日志平台有机会拿到相关堆栈：

```
t@369:1556
constructClassInstance@44:60648
...
t@44:73307
updateContainer@44:146914
render@44:78774
...
```

由于js代码发布的时候都会经过[uglify-es](https://github.com/mishoo/UglifyJS2.git#harmony)的代码混淆，得到的堆栈根本看不出可用的函数名称以及所在文件路径，所以即使非常熟悉js业务代码人员面对以上堆栈也是无法定位问题所在。
既然有混淆，那么应该有反混淆的手段[source-map-cli](https://github.com/gabmontes/source-map-cli)，metro已经做了支持，即在打bundle的时候通过打包参数`--sourcemap-output`指定一个map文件，该参数会传递到metro，然后metro在打bundle过程中会额外打出一个与之对应到map文件，该map文件包含反混淆的所有信息。假设common.bundle对应的map文件未common.bunde.map，对于`common.bundle:44:60648`这一行堆栈信息，可以使用以下方式进行反混淆：

```Shell
source-map resolve common.bundle.map 44 78774
```

map文件结合行列信息，将定位到原始module文件以及行列：

```Shell
Maps to XPMotors_ReactNative/node_modules/react-native/Libraries/Renderer/ReactNativeRenderer-prod.js:6464:18
```

metro中增加RN分包功能之后，会影响生成map文件的逻辑，故需要增加额外逻辑支持js异常解析。
前面提到过，在打common bundle的时候，会额外引入一个common.js的入口文件，如果不做任何处理，该入口文件最终会作为一个module放入到common.bundle。实际上，该module是不应该出现在common.module的，并且在运行业务bundle过程中会报错。因此，需要在生成common bundle的过程中删除入口文件对应的module。另外，还需要在生成common bundle的map文件的过程中删除入口文件对应的module。篇幅关系这里就不在代码实现上进一步展开了。

### 5.5 后续计划

- 打多个common bundle
- 业务按需加载
- 自动解析js异常堆栈
- 多进程下共享common bundle
- 调试模式下的分包

目前对metro对修改只支持打一个common bundle，后面可以考虑支持打多个不同的common bundle。理想情况是，首先加载RN相关的common bundle，假设业务A有自己的baseA bundle，允许加载baseA bundle，最后才是业务A自己的bundle。如果业务A需要放在首屏，那么不必加载一大堆不相干的基础代码。在后续加载业务B的时候，业务B可以加载自己的baseB bundle。当然，如果业务A和业务B都依赖同一个base bundle，有两个选择：放在common bundle或者单独打出base bundle，在加载A或者B的时候需要外部管理好bundle的依赖，自动去加载base bundle。
业务按需加载有两种，一种是native层管理业务加载时机，一种是js层自己判断加载时机，后面会深入去了解。
虽然RN分包方案支持js异常解析，但可以更进一步：打通日志后台，做到js异常日志自动解析，提供异常日志告警等功能。
多进程下共享common bundle主要针对车载大屏，允许zygnote进程启动过程中加载RN并运行common bundle。
另一个待改进的问题，需要在RN调试模式下支持RN分包，目前在RN调试模式下使用的还是同一个bundle，工作量会比较大。

## 6. 最后，来点干货

***授人以鱼，不如授人以渔。***

RN分包关键在于metro的改造，本文没有一开始去分析metro的整体框架，分析完估计还是一脸懵，估计自己也一时分析不清楚。退而求其次，抓住metro的关键流程，然后作出相关改造。在梳理关键流程过程中，多多少少对整体架构有所了解。

在梳理js代码相关流程的时候，如果底层是v8引擎并且可以更改v8 binding代码的话，个人习惯在binding中增加profile相关代码，这样可以抓取相关运行堆栈。有了堆栈，那么就很容易抓住关键流程了。

如果是运行在node里面，node有一个非常好用的profile工具[v8-profiler](https://github.com/node-inspector/v8-profiler)，只需要在代码入口增加相关profile代码就可以得到完整运行堆栈：

```Javascript
const profiler = require('v8-profiler-next')
const title = '';
profiler.startProfiling(title, true);
// 这里是待profiling的函数
setTimeout(() => {
  var profile1 = profiler.stopProfiling(title);
  profile1.export(function(error, result) {
    fs.writeFileSync('build.cpuprofile', result);
    profile1.delete();
  });
}, 1500);
```

运行以上代码，如果本地存在build.cpuprofile这个文件，恭喜你！

还有最后一步，如果查看build.cpuprofile这个文件？

这里提供另一个非常好用的工具[traceviewify](https://github.com/thlorenz/traceviewify.git)，该工具会将cpuprofile格式文件转换成json文件，然后就可以在chrome浏览器中打开chrome://tracing并加载json文件:

![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/jstrace.png)

有图有真相，enjoy！
