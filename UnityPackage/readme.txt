MediaDecoder for Unity - v1.0.1

Quick start:
0.	Download the FFmpeg 3.0.1 Win64 Shared from https://ffmpeg.zeranoe.com/blog/?p=499#more-499
1.	Put the following dlls to project folder(the same directory with Assets or beside *.exe)
	- avcodec-57.dll
	- avformat-57.dll
	- avutil-55.dll
	- swresample-2.dll
2.	Create a model with MeshRenderer(ex.Quad) and attach MediaDecoder.cs as component.
3.	Set MeshRenderer’s Material to YUV2RGBA and make sure the shader to be YUV2RGBA.
4.	Fill in video path(ex. D:\_Video\sample.mp4) and enable Play On Awake.
5.	Click play, now you should be able to see the video playing on the model.

Requirements:
- The plugin currently only supports Windows / DX11.

API lists:
- bool isVideoEnabled:
	(Read only) Video is enabled or not. This value is available after initialization.

- bool isAudioEnabled
	(Read only) Audio is enabled or not. This value is available after initialization.

- float videoTotalTime
	(Read only) Video duration. This value is available after initialization.

- float audioTotalTime
	(Read only) Audio duration. This value is available after initialization.

- void initDecoder(string path):
	Start a asynchronized initialization coroutine. onInitComplete event would be invoked when initialization was done.

- void startDecoding():
	Start native decoding. It only works for initialized state.

- void replay():
	Replay the video.
	
- void stopDecoding():
	Stop native decoding process. It would be called at OnApplicationQuit and OnDestroy.

- void setSeekTime(float seekTime):
	Seek video to given time. Seek to time zero if seekTime over video duration.
	
- bool isSeeking():
	Return true if decoder is processing seek.
	
- void setStepForward(float sec):
	Seek based on current time. It is equivalent to setSeekTime(currentTime + sec).
	
- void setStepBackward(float sec):
	Seek based on current time. It is equivalent to setSeekTime(currentTime - sec).
	
- void getVideoResolution(ref int width, ref int height):
	Get video resolution. It is valid after initialization.
	
- float getVideoCurrentTime():
	Get video current time(seconds).
	
- DecoderState getDecoderState():
	Get decoder state. The states are defined in MediaDecoder.DecoderState.
	
- void setPause():
	Pause the video playing. It is available after initialization.
	
- void setResume():
	Resume from pause. It is available only for pause state.
	
- void setVolume(float vol):
	Set the video volume(0.0 ~ 1.0). Default value is 1.0.
	
- float getVolume():
	Get the video volume.
	
- void mute():
	Mute video. It is equivalent to setVolume(0.0f).
	
- void unmute():
	Unmute video. It is equivalent to setVolume(origianlVolume).
	
- static void getMetaData(string filePath, out string[] key, out string[] value):
	Get all meta data key-value pairs.
	
Tools and sample implement:
- UVSphere.cs:
	Generate custom resolution UV-Sphere. It is useful for 360 video player.

- FileSeeker.cs:
	Used to get file path sequentially from a folder.
	
- StereoHandler.cs:
	A tool to set texture coordinates for stereo case.

- StereoProperty.cs:
	A data class to describe stereo state.
	
- VideoSourceController.cs:
	Sample implement to load video one by one from a folder.
	
- StereoVideoSourceController.cs:
	Stereo version of VideoSourceController.
	
- ImageSourceController.cs:
	Sample implement to load image one by one from a folder.
	
- StereoImageSourceController.cs:
	Stereo version of ImageSourceController.

Scenes:
- SampleScene.unity:
	A sample to play video on a quad by following the quick start.

- Demo:
	Demo some use cases which include video, stereo video, image, stereo image and 360 video.
	This demo has dependency with SteamVR plugin.