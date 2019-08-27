package xpeng.com.facebook.react.tracker;

/* XPENG_BUILD_TRACKER */
import android.os.Bundle;

import com.facebook.proguard.annotations.DoNotStrip;

@DoNotStrip
public interface TrackerListener {

  @DoNotStrip
  class Message {
    private String mCategory;
    private String mSubCategory;
    private Bundle mPayload;
    //TODO: obtain message from pool

    public Message(String category, String subCategory, Bundle payload) {
      mCategory = category;
      mSubCategory = subCategory;
      mPayload = payload;
    }

    public String getCategory() {
      return mCategory;
    }

    public String getSubCategory() {
      return mSubCategory;
    }

    public Bundle getPayload() {
      return mPayload;
    }
  }

  void onTrack(Message message);
}

/* XPENG_BUILD_TRACKER */
