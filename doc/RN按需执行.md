## 1. 背景
RN业务性能相比原生还是有差距的，即使将业务bundle缓存到本地加载，由于RN渲染机制的限制，在真正构建native ui之前需要执行大量js代码，造成不好的白屏等待体验。执行大量js代码带来几个影响：

* js编译耗时增加，增大启动耗时
* 预加载js情况下，执行太多js代码增大启动耗时
* 预加载js情况下，执行太多js会创建很多不必要的js对象，给启动内存带来压力

而如何减少执行的js代码体积，成为一个主要的优化手段。

RN分包之后，将原本一个完整的bundle拆分为common bundle和business bundle。在app启动阶段预加载common bunde，构建RN相关的js执行环境。在业务入口开始加载并执行业务bundle代码，这部分业务bundle代码会比之前小很多，故白屏等待时长也会减少很多，甚至可以提前预加载重要业务bundle，将白屏等待时长降低为零。但是，如果将业务入口提前到app首页的话，相当于app启动过程中完整执行一次common bundle和business bundle，这个过程耗时跟没有分包之前是一样的，执行代码没有减少。

随着越来越多的业务选择RN来实现，业务bundle的体积以不可见预期增加。业务bundle越来越大，只会给RN业务性能带来更加严峻的挑战。

那么，如何减少执行js代码体积呢？RN分包之后，是否有其他优化方案？

注1: 本文档提到的RN版本基于0.53.3

## 2. RN业务按需加载
RN业务按需加载这个方案用来减少执行js代码体积。

### 2.1 基本思想
对RN业务打包模块metro进行改造，以模块为单位将一个业务bundle拆分为多个子bundle，子bunde包含RN相关的基础bundle，包含跟业务相关的基础bundle以及真正跟业务逻辑结合紧密的真正的业务bundle。此时的业务bundle依赖于打出的几个基础bundle，RN在加载业务bundle之前会首先加载依赖的几个基础bundle。
以App目前的业务bundle为例，业务bundle包含了很多其他业务代码，比如社区、商城、钱包、充电等等。将社区业务放到首页的时候，实际上业务bundle包含了很多跟社区业务无关的js代码。按需加载之后，只用会加载跟社区相关的代码，达到减少执行js代码体积的目的。

### 2.2 方案选型
提到按需加载，熟悉js模块思想的伙伴肯定会想到[CMD](https://github.com/cmdjs/specification/blob/master/draft/module.md)模块规范，鼓励延迟加载依赖模块，在真正使用到模块的时候去加载：

```Javascript
define(function(require, exports) {

  // 获取模块 a 的接口
  var a = require('./a');

  // 调用模块 a 的方法
  a.doSomething();

});
```

如上，通过define定义了一个模块，该模块依赖于模块a，并没有一开始就加载模块a，而是在需要使用到模块a的接口的时候才去加载并生成模块a对象。

RN打包使用的是[AMD](https://github.com/amdjs/amdjs-api/blob/master/AMD.md)来管理模块加载依赖的，而AMD推崇前置加载所有依赖。大家可以看到RN的bundle里面，require入口module之前通过`__d`定义了一大堆的模块，并且`__d`里面列出了该模块所依赖的其他模块，保证依赖模块全部加载完毕才会去构造被依赖模块。通过修改RN内部require的native实现，可以做到类似CMD的延迟加载依赖。但是需要业务代码使用以上的代码写法。比如，业务A拆分出了common，base business以及business这三个bundle，那么需要业务在业务逻辑中写类似代码：


```
define(function(require, exports) {
  if (common bundle didn't loaded)
    require('common.bundle');
  if (base business bundle didn't loaded)
    require('base business bundle')

  require('business bundle')
});
```

可以看到，业务代码里面耦合了加载分包bundle的代码。对于业务代码而言，只会关心具体的业务module，现在倒好了，还需要关心加载RN分包bundle的代码，不熟悉RN分包的开发者看到以上代码也许会纳闷base business bundle是哪一个模块，为什么需要加载该模块诸如此类疑问。

基于以上考虑，本文不会采用类似CMD的延迟加载方案，不会将加载bundle的逻辑耦合到业务逻辑之中。为了让RN业务开发方不需要关心如何加载bundle，本文提供以下两种可行方案：

* 精细的bundle拆分：将一个bundle拆分成多个子bundle，通过dependency map来明确子bundle之间的依赖关系，native端实现子bundle端的加载和依赖管理。
* unbundle：RN提供的一种以module为单位的细粒度bundle拆分方案，但要解决多js文件导致的I/O性能问题。

其中，精细的bundle拆分实现上稍复杂，需要对metro流程进行改造以及在native端实现bundle加载管理，Android和ios都可以实现。而RN提供的unbundle方案Android和ios实现方式不一样，均在不同程度上存在性能问题，后面会讨论针对Android和ios上的进一步优化方案。

## 3. 精细的bundle拆分

![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/bundle.png)

简单回顾之前的RN分包方案，一个bundle一分为二：common + business。其中，common bundle对应的打包入口文件common.js，打包过程中会得到common modules并将这些module的path和id信息序列化到本地，然后根据common modules生成common bundle。business bundle由业务入口文件打包而成，打包过程中依赖于序列化的common modules信息，筛选出business modules并最终生成business bundle。

而本文的RN业务按需加载，在RN分包方案的基础之上，需要实现更加精细的bundle拆分。如上图所示，common bundle也要求能够拆分出多个子bundle，business bundle也要求能够拆分出多个子bundle。对于所有子bundle，需要通过dependency map来明确彼此之间的加载依赖。

### 3.1 打包流程优化
![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/metro%20bundle.png)

基于RN分包打包流程优化的实现，由打包参数`--common-build`指定当前打出common的子bundle，`--business-bundle`指定当前打出business的子bundle。不同的是，打包每个子bundle的时候都会序列化相关modules的path和id信息到本地。下一次打某个子bundle的时候，需要利用之前序列化的所有modules信息，这样才能筛选出当前打包入口文件引入的modules。

### 3.2 生成dependency map
在方案选型小节提到RN是使用[AMD](https://github.com/amdjs/amdjs-api/blob/master/AMD.md)来管理模块加载依赖。metro会在打包过程中通过`__d`定义每个module，并计算该module所依赖其他module列表，并将这些依赖module的id列表编码到当前module里面。在通过`require`加载某个module的时候，会根据id列表自动加载依赖module。

RN业务按需加载方案考虑的是bundle级别的依赖，而非module级别的依赖。因此，需要寻找一种自动能够计算bundle依赖的方式，并将bundle依赖信息（dependency map）序列化到文件bundle.deps文件。

在RN分包方包方案里面有描述过如何生成business modules，上面流程图提到的筛选出当前modules的过程与之类似。打包过程首先根据业务入口文件生成一个modules列表A，包含当前入口文件代表的业务所依赖的所有modules，跟当前业务紧密相关的module列表B有待筛选。筛选过程中，需要加载所有modules的序列化文件，生成多个<module path, module id>的map对象。有了这些map之后，筛选过程比较简单：遍历列表A取得module id，然后遍历每个map，如果所有map中都不存在这个module id，则将该module添加到列表B。而计算bundle依赖的逻辑简单明了：如果某个map中存在这个module id，则表明当前业务bundle依赖于该module所在的bundle。

### 3.3. bundle加载管理
![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/BundleManager.png)

ReactBundleManager负责管理Bundle的加载和运行，ReactBundleDeps负责解析bundle.deps并生成bundle依赖关系。

## 4. unbundle
RN打包支持unbundle打包命令，每个module都会单独打在一个文件里面，module里面通过require来加载依赖的其他module。本小节介绍unbundle基本原理以及unbundle的优缺点，并提出优化方案。

### 4.1 unbundle原理
通过以下命令打unbundle：

```
react-native unbundle --entry-file [enntry file] --bundle-output x.bundle --platform android
```

此时打出的x.bundle里面可能只是一个简单的require语句：

```
require(12)
```

并且多了一个js-modules文件夹，里面全都是各个module的js文件，当然肯定包含`12.js`这个文件，并且多了一个`UNBUNDE`文件。通过unbundle打包打出来的bundle采用了[CMD](https://github.com/seajs/seajs/issues/242)来管理模块，支持就近依赖，按需执行。以RN社区业务为例，如果入口文件里面只包含社区相关的代码的话，那么打出的社区bundle只会require跟社区相关的module，达到减少可执行js代码大小的目的。

RN的native端加载bundle的时候，会判断该bundle所在目录里面是否存在js-modules目录，如果存在则进一步判断js-modules里面是否存在UNBUNDLE并进行内容校验，判断是非等于`0xFB0BD1E5`预定义的magic数字，如果合法则认为加载的bundle属于unbundle，从而启动unbundle的加载流程。在执行js端的require代码的时候，js端有一个modules数组来管理已加载的module。js端的require会调用RN的native端的require，会根据require的参数到js-modules文件夹里面去加载对应的js文件。

### 4.2 unbundle优缺点
unbundle这种拆分bundle的方案拆分粒度以modules为单位，好处是直接利用require来管理模块加载，不需要额外维护加载依赖。由于采用CMD模块管理，故可以很好的支持业务按需加载，本文认为这是unbundle的精华所在。

unbundle是RN官方支持的拆分方案，维护成本有优势，遇到问题也可以去社区看是否有解决方案。

稍微复杂点的业务js-modules里面至少包含数千个module，这样会有数千多个小文件，通过require去加载的话存在大量I/O导致的性能问题。那么如何解决这个性能问题呢？

## 5. 基于unbundle的精细bundle拆分
本节先讨论一下如何解决unbundle导致的大量I／O问题，然后提出基于unbundle的bundle拆分方案。

### 5.1 I/O性能解决
在ios上面通过unbundle打出的bundle没有js-modules，而是将所有小的modules文件内容写到同一个bundle文件，并且在bundle的头部插入二进制协议来标示每个module的开始和结束位置，这样在require的时候通过fseek来获取模块内容。幸运的是，在Android上面也可以打出类似ios上面的这种bundle，这样可以解决大量I/O导致的性能问题，只需要在打包的时候增加一个`--indexed-unbundle`参数：

```
react-native unbundle --entry-file [enntry file] --bundle-output x.bundle --platform android --indexed-bundle
```

以上打包会将entry file引入所有module打在同一个bundle，加载bundle的时候一次I/O将bundle读到内存，然后所有modules都在内存里面，后续require直接从内存加载module，避免大量I/O。

### 5.2 bundle拆分
如果采用unbundle方式来打bundle的话，bundle会以module为单位进行了划分，拆分的粒度非常细，为什么还要进行bundle拆分呢？

以App目前打bundle为例进行说明，目前App里面的所有RN业务都是打成一个business bundle，使用的是一个打包入口文件，该入口文件会import所有的业务入口，只要这样才能通过metro打包分析所有业务的module依赖，并最终将所有module代码打到一个bundle里面。为了支持业务按需加载，需要多个打包入口文件打出多个不同的子bundle，并且多个业务之间要高度解耦，解除不同业务之间的require依赖，这样才能减少执行代码大小的目的，实现RN业务的按需加载。

### 5.3 common unbundle
对于包含react／react native等公用的common modules，是否有必要通过unbundle进行打包呢？

之前的RN分包将bundle拆分为common bundle和business bundle，实际上可以对common bundle沿用bundle打包方式，采用AMD方式来管理模块，而business bundle沿用unbundle打包方式，并且增加`--indexed-unbundle`打包参数打成一个bundle。但本文更加推荐对common bundle采用unbundle打包方式。

回头看本文第三节的精细bundle拆分那张图，将一个bundle拆分成几个子bundle之后，不可避免的需要管理各个子bundle之间的加载依赖。如果common bundle沿用bundle打包方式打多个子bundle的话，需要在打包的时候生成dependency map并且native端根据map来管理多个bundle之间的加载。但是，如果common bundle使用unbundle的打包方式打成多个子bundle，只需要一开始将多个子bundle全部加载到内存。那么js端调用require来加载某个module的时候，实现对内存中的多个二进制bundle头部的遍历，找到对应的module并返回module的内容即可。对common也采用unbundle打包正是利用了unbundle打包的优点，直接通过require来管理module依赖，不需要native额外负责各个子bundle之间的加载依赖。

### 5.4 unbundle二进制头部格式
![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/unbundle.png)

unbundle文件格式由四个部分组成，header和modules table是二进制格式，startup code和modules跟常规方式打出的bundle没有差别，都是可运行的js代码。

RN的native端在加载unbundle文件时需要解析header跟modules table。当判断header的前四个字节的值为magic number（0xfb0bd1e5）的话，则判断为unbundle文件格式。

### 5.5 require流程
![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/require.png)

require流程由js端发起，比如startup code里面require一个入口文件，会触发RN的native端去开始加载入口文件的module，并执行module代码，从而js端就可以拿到module.exports对象。假设require(12)，其代码为：

```
__d(function (global, _require, module, exports, _dependencyMap) {
  'use strict';
  if (process.env.NODE_ENV === 'production') {
    module.exports = _require(_dependencyMap[0], './cjs/react.production.min.js');
  } else {
    module.exports = _require(_dependencyMap[1], './cjs/react.development.js');
  }
}, 12, [13,17],"node_modules/react/index.js");
```

那么native端会执行以上代码，并给module.exports赋值，代表该module对外提供的所有变量和方法。以上代码还清楚显示了，该module依赖的module（id分别为13和17）会自动require。

以上require流程中，native端的JSIndexedRAMBundle负责解析某个undundle，并根据moudle id来查找并返回对应module的代码；RAMBundleRegistry从命名上看是unbundle文件注册管理的作用，但只允许注册一个unbundle文件，并且只支持从文件系统来加载unbundle文件。

以上简单介绍了require在js端和react-native的native端的整体流程，方便理解后面章节介绍的实现从多个unbundle文件中去require某个module。

## 6. react-native改造
RN的版本是0.55.3，已经支持unbundle，但只支持从文件系统里面加载一个unbundle文件。由于App不支持RN热更新功能，采用从assets加载bundle文件的方案，需要扩展unbundle的加载流程，支持从assets来加载unbundle文件。

另外，RN只支持加载一个unbundle文件，虽然RAMBundleRegistry支持注册新的module，但只支持这些module来自某个代码片段（Segment），即只支持registerSegment的操作，不支持注册某个独立的unbundle文件。故本方案在CatalystInstance中增加了一个注册方法registerSubUnbundleFromAssets()，注册流程过程中会解析所有这些子unbundle文件，并创建对应的JSIndexedRAMBundleString对象（注：该对象是新增的，RN原生里面没有，代表从字节流构建的unbundle对象），并将该对象注册到RAMBundleRegistry。

关于注册时机，在加载common unbundle之后就需要调用registerSubUnbundleFromAssets()注册所有子unbundle。

关于require流程，通过某个module id查找module的时候，需要从多个unbundle来遍历查找module，具体修改是在RAMBundleRegistry::getModule()，支持从注册的unbundle对象列表里面查找module。

还有其他修改，比如对于unbundle文件的二进制格式的解析的扩展，后面在metro改造的过程中会提到。

## 7. metro改造
现在打common unbundle命令：

```
react-native unbundle --entry-file common.js --bundle-output main.unbundle --platform android --indexed-bundle --split true --remove-entry true --reset-module-id true
```

如上，去掉了RN分包引入的两个打包参数`--common-build`以及`--business-build`，使用打包开关`--split`来代替。`--remove-enrty`表示不要在unbundle文件中为common.js生成module，因为common.js不是一个真正的业务入口文件。如果是一个真正的业务入口文件，`--remove-entry`需要设置为true。

根据业务入口文件打子unbundle的命令：

```
react-native unbundle --entry-file [业务入口js文件] --bundle-output xx.unbundle --platform android --indexed-bundle --split true --remove-entry false --reset-module-id false
```

如果是一个真正的业务入口文件，则`remove-entry`为false。例如，如果要想把多个业务公共的业务代码打在一个unbundle，那么其业务入口就不算一个真正的业务入口，那么在打该unbundle的时候`remove-entry`为true。

关于` --reset-module-id`，当需要从零开始生成module id时候就需要将其置为true。一般来说只在打main.unbundle的时候该参数才为true，打业务unbundle的时候都为false。

有了RN分包作为基础，对metro的改造比较简单，只不过在打每次unbundle的时候，都需要将相关的module id信息序列化下来，打下一次unbundle的时候读取所有序列化信息。

另外，需要对unboundle二进制格式进行扩展，头部增加一个字段用来表示当前unbundle文件里面最小的module id，由于unbundle头部已经包含了module个数，并且同一个unbundle文件内所有module的id是连续的，这样就知道当前unbundle文件内部所有module所在的id区间。在后续require流程中通过某个id查找module的时候，很容易判断该id是否存在于某个unbundle文件所在id区间。


## 8. 初步成果
给App的RN业务进行了初步拆分：

![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/unbundle-split.png)

* main.unbundle为RN核心库相关的unbundle
* business-bbs.unbundle为社区业务
* business-charge.unbundle为充电业务
* business-MallGoods.unbundle为商城业务
* business-service.unbundle为车主服务业务

相关之前的单个bundle，本文在js引擎编译相关js code的地方（JSEvaluateScript()函数）进行了时间统计，以下三种机型上面都获得了一定性能提升：

| | 单独bundle | 按需加载unbundle |
| --- | --- | --- |
| Mi 4(rom 4.4.4) | 2175ms | 1680ms |
| Nexus 5(rom 4.4.2) | 1650ms | 1250ms |
| Mate 9(rom 7.0) | 715ms | 550ms |

App的RN业务的unbundle拆分需要由前端开发小伙伴进行精细拆分，如果可以保证首页启动过程只加载跟社区相关的module的话，那么性能提升会更加明显。

在JSC／V8等主流js引擎上，编译code大小每增加1M，则编译时长会增加1秒。基于unbundle拆分的方案可以保证编译code保持在1M左右的大小，保证RN业务加载性能始终保持在合理水准，给业务带来如下明显的好处：

![](https://github.com/brunoduan/react-native-run-on-demand/blob/develop-v0.59.10/doc/images/unbundle-change.png)


## References

https://zhuanlan.zhihu.com/p/35789551
http://blog.desmondyao.com/rn-split/
https://github.com/pukaicom/reactNativeBundleBreak
https://ericroz.wordpress.com/2017/02/24/unbundling-react-native-unbundle/
https://github.com/zhangyu921/blog/issues/7
