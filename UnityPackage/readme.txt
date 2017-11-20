ViveMediaDecoder for Unity - v1.1.5

Quick start:
0.	Download the FFmpeg 3.4:
	- 64 bits: https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-3.4-win64-shared.zip
	- 32 bits: https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-3.4-win32-shared.zip
	Please notice that the license of FFmpeg official build is GPL and https is not default enabled.
1.	Put the following dlls to the plugins folder(ex. If your project is at D:\SampleProject\,
												 you can put the dlls to D:\SampleProject\Assets\ViveMediaDecoder\Plugins\x64\)
	- avcodec-57.dll
	- avformat-57.dll
	- avutil-55.dll
	- swresample-2.dll
2.	Create a model with MeshRenderer(ex.Quad) and attach ViveMediaDecoder.cs as component.
3.	Set MeshRenderer’s Material to YUV2RGBA and make sure the shader to be YUV2RGBA.(YUV2RGBA_linear is for linear color space)
4.	Fill in video path(ex. D:\_Video\sample.mp4) and enable Play On Awake.
5.	Click play, now you should be able to see the video playing on the model.
6.	If dll not found, check the followings:
	- In editor mode:
		* Make sure your Unity editor and FFmpeg dlls are consistent with either 32 bits or 64 bits.
		* Make sure FFmpeg dlls are placed in correct position.
		Ex. If your project is located in D:\Sample\, the following directories should work:
		D:\Sample\Assets\ViveMediaDecoder\Plugins\x86_64\ or D:\Sample\Assets\ViveMediaDecoder\Plugins\x86\
		C:\Program Files\Unity\Editor\
		Other system path in environment variables.
	- Standalone build:
		* Make sure your build settings and FFmpeg dlls are consistent with either 32 bits or 64 bits.
		* Remember to copy FFmpeg dlls to the directory where the build can find .
		Ex. If your project is built to D:\Build\SampleApp.exe, the following directories should work:
		D:\Build\
		D:\Build\SampleApp_Data\Plugins\
		Assign library loading path in your project.(please refer to Environment.SetEnvironmentVariable)
		Other system path in environment variables.

Requirements:
- The plugin currently only supports Windows / DX11.

API lists:
- bool isVideoEnabled:
	(Read only) Video is enabled or not. This value is available after initialization.

- bool isAudioEnabled:
	(Read only) Audio is enabled or not. This value is available after initialization.

- float videoTotalTime:
	(Read only) Video duration. This value is available after initialization.

- float audioTotalTime:
	(Read only) Audio duration. This value is available after initialization.
	
- int audioFrequency:
	(Read only) Audio sample rate. This value is available after initialization.

- int audioChannels:
	(Read only) Audio sample rate. This value is available after initialization.

- void initDecoder(string path, bool enableAllAudioCh = false):
	Start a asynchronized initialization coroutine. onInitComplete event would be invoked when initialization was done.
	Set enableAllAudioCh to true if you want to process all audio channels by yourself. In this case, the audio would not be play by default.

- void startDecoding():
	Start native decoding. It only works for initialized state.

- void replay():
	Replay the video.
	
- void stopDecoding():
	Stop native decoding process. It would be called at OnApplicationQuit and OnDestroy.

- void setVideoEnable(bool isEnable):
    Enable/disable video decoding. 
	
- void setAudioEnable(bool isEnable):
	Enable/disable audio decoding. 
	
- void getAllAudioChannelData(out float[] data, out double time, out int samplesPerChannel):
	Get all audio channels. This API could only be used when the flag enableAllAudioCh is true while initialization.
	
- void setSeekTime(float seekTime):
	Seek video to given time. Seek to time zero if seekTime over video duration.
	
- bool isSeeking():
	Return true if decoder is processing seek.

- bool isVideoEOF():
	Return true if decoder is reach end of file.
	
- void setStepForward(float sec):
	Seek based on current time. It is equivalent to setSeekTime(currentTime + sec).
	
- void setStepBackward(float sec):
	Seek based on current time. It is equivalent to setSeekTime(currentTime - sec).
	
- void getVideoResolution(ref int width, ref int height):
	Get video resolution. It is valid after initialization.
	
- float getVideoCurrentTime():
	Get video current time(seconds).
	
- DecoderState getDecoderState():
	Get decoder state. The states are defined in ViveMediaDecoder.DecoderState.
	
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
	
- static void loadVideoThumb(GameObject obj, string filePath, float time):
	Load video thumbnail of given time. This API is synchronized so that it may block main thread.
	You can use normal decoding process to leverage the asynchronized decoding.
	
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
	
- config:
	Decoder config. If you want to adjust buffer size or use TCP for RTSP, put it to the directory that can be found(the same with FFmpeg).
	Use default settings if there is no config.

Scenes:
- SampleScene.unity:
	A sample to play video on a quad by following the quick start.

- Demo:
	Demo some use cases which include video, stereo video, image, stereo image and 360 video.
	Please follow the steps below to make it work properly:
	1. Add two layers named LeftEye and RightEye.
	2. Found the objects named begining with "Stereo", set it's children's layer to LeftEye and RightEye.
	3. Set the Camera (left eye)'s Target Eye to Left, Culling Mask to uncheck RightEye.
	4. Set the Camera (right eye)'s Target Eye to Right, Culling Mask to uncheck LeftEye.
	5. Modify the directory of each demo to your own path and click play.

Change for v1.1.5:
- Modify native decoding thread number to auto detection.
- Modify document for using FFmpeg 3.4 for the issue of playback broken building with Unity 2017.2f under Windows 10.
	
Change for v1.1.4:
- Rename to ViveMediaDecoder.
	
Change for v1.1.3:
- Fix thumbnail loading crash.

Change for v1.1.2:
- Improve software decoding performance by multi-thread decoding.
- Reduce audio playing artifact by enlarge overlap length.
- Modify FFmpeg dlls directory description in readme.
	
Change for v1.1.1:
- Fix seek function.
	
Change for v1.1.0:
- Modify native buffer management.
- Modify audio play process for lower scheduling delay.
- Open native config for seek function and buffer settings.
	
Change for v1.0.7:
- Add native config file for buffer size control, use TCP if streaming through RTSP.	
- Fix RTSP playing fail.

Changes for v1.0.6:
- Fix buffering issue.

Changes for v1.0.5:
- Fix the dll loading issue for 32-bits Unity editor.
- Fix the occasionally crash when call seek function.
- Fix the issue that onVideoEnd event is not invoked correctly.
- Add EOF decoder state and API.
- Add API to get all audio channels.
- Add thumbnail loading API.
- Improve seek performance.
- Set the index of FileSeeker to public(read only) and modify API toPrev and toNext to return index.

Changes for v1.0.4:
- Add API for video/audio enable/disable decoding.

Changes for v1.0.3:
- Add a shader for linear color space.
- Modify the sample scene for Unity 5.4.

Changes for v1.0.2:
- Support streaming through https.