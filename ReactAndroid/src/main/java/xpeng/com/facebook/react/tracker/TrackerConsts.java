package xpeng.com.facebook.react.tracker;

/* XPENG_BUILD_TRACKER */
import com.facebook.proguard.annotations.DoNotStrip;

@DoNotStrip
public class TrackerConsts {

  @DoNotStrip
  public enum Category {
    react("P00002");

    private String mName;

    Category(String name) {
      mName = name;
    }

    @Override
    public String toString() {
      return mName;
    }
  }

  @DoNotStrip
  public enum SubCategory {
    sld("B001"),
    rc("B002"),
    bld("B003"),
    br("B004"),
    cv("B005");

    private String mName;

    SubCategory(String name) {
      mName = name;
    }

    @Override
    public String toString() {
      return mName;
    }
  }

  @DoNotStrip
  public enum KV {
    key_sld_begin_time("begin-time"),
    key_sld_end_time("end-time"),
    key_rc_begin_time("begin-time"),
    key_rc_end_time("end-time"),
    key_bld_begin_time("begin-time"),
    /* bundle load is asynchronous, so key_bld_end_time is meaningless */
    key_bld_end_time("end-time"),
    key_br_begin_time("begin-time"),
    /* key_br_end_time is hard to check it out */
    key_br_end_time("end-time"),
    key_bld_url("name"),
    key_enter_url("url"),
    key_cv_activity("string-activity"),
    key_cv_activity_time("long-activity-time"),
    key_cv_view_time("time"),
    key_cv_count("int-count");

    private String mName;

    KV(String name) {
      mName = name;
    }

    @Override
    public String toString() {
      return mName;
    }
  }
}
/* XPENG_BUILD_TRACKER */
