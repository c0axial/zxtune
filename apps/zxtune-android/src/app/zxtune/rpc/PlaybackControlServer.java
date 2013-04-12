/*
 * @file
 * @brief Remote server stub for Playback.Control
 * @version $Id:$
 * @author (C) Vitamin/CAIG
 */

package app.zxtune.rpc;

import java.util.concurrent.TimeUnit;

import android.net.Uri;
import app.zxtune.Playback;

public class PlaybackControlServer extends IPlaybackControl.Stub {

  private final Playback.Control delegate;

  public PlaybackControlServer(Playback.Control delegate) {
    this.delegate = delegate;
  }

  @Override
  public ParcelablePlaybackItem getItem() {
    return ParcelablePlaybackItem.create(delegate.getItem());
  }

  @Override
  public long getPlaybackPosition() {
    return delegate.getPlaybackPosition().convertTo(TimeUnit.MILLISECONDS);
  }
  
  @Override
  public String getStatus() {
    return delegate.getStatus().toString();
  }
  
  @Override
  public void playItem(Uri item) {
    delegate.play(item);
  }

  @Override
  public void play() {
    delegate.play();
  }

  @Override
  public void pause() {
    delegate.pause();
  }

  @Override
  public void stop() {
    delegate.stop();
  }
}