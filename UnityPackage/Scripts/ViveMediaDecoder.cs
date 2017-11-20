//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;
using UnityEngine.Events;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.ComponentModel;

namespace HTC.UnityPlugin.Multimedia
{
	[RequireComponent(typeof(MeshRenderer))]
	public class ViveMediaDecoder : MonoBehaviour
	{
		private const string NATIVE_LIBRARY_NAME = "ViveMediaDecoder";

        //  Decoder
        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern int nativeCreateDecoder(string filePath, ref int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern int nativeCreateDecoderAsync(string filePath, ref int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern int nativeGetDecoderState(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeCreateTexture(int id, ref IntPtr tex0, ref IntPtr tex1, ref IntPtr tex2);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeStartDecoding(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeDestroyDecoder(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsVideoBufferFull(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsVideoBufferEmpty(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsEOF(int id);

        //  Video
        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsVideoEnabled(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeSetVideoEnable(int id, bool isEnable);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeGetVideoFormat(int id, ref int width, ref int height, ref float totalTime);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeSetVideoTime(int id, float currentTime);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsContentReady(int id);

        //  Audio
        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsAudioEnabled(int id);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeSetAudioEnable(int id, bool isEnable);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeSetAudioAllChDataEnable(int id, bool isEnable);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeGetAudioFormat(int id, ref int channel, ref int frequency, ref float totalTime);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern float nativeGetAudioData(int id, ref IntPtr output, ref int lengthPerChannel);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeFreeAudioData(int id);

        //  Seek
        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern void nativeSetSeekTime(int id, float sec);

        [DllImport(NATIVE_LIBRARY_NAME)]
        private static extern bool nativeIsSeekOver(int id);

        //  Utility
        [DllImport (NATIVE_LIBRARY_NAME)]
		private static extern int nativeGetMetaData(string filePath, out IntPtr key, out IntPtr value);

        [DllImport (NATIVE_LIBRARY_NAME)]
        private static extern void nativeLoadThumbnail(int id, float time, IntPtr texY, IntPtr texU, IntPtr texV);

        //  Render event
        [DllImport (NATIVE_LIBRARY_NAME)]
		private static extern IntPtr GetRenderEventFunc();

        private const string VERSION = "1.1.5.170807";
		public bool playOnAwake = false;
		public string mediaPath = null;	            //	Assigned outside.
		public UnityEvent onInitComplete = null;    //  Initialization is asynchronized. Invoked after initialization.
		public UnityEvent onVideoEnd = null;        //  Invoked on video end.

		public enum DecoderState {
			INIT_FAIL = -2,
			STOP,
			NOT_INITIALIZED,
			INITIALIZING,
			INITIALIZED,
			START,
			PAUSE,
			SEEK_FRAME,
            BUFFERING,
            EOF
		};
		private DecoderState lastState = DecoderState.NOT_INITIALIZED;
		private DecoderState decoderState = DecoderState.NOT_INITIALIZED;
		private int decoderID = -1;

		private const string LOG_TAG = "[ViveMediaDecoder]";

		public bool isVideoEnabled { get; private set; }
		public bool isAudioEnabled { get; private set; }
        private bool isAllAudioChEnabled = false;

        private bool useDefault = true;             //  To set default texture before video initialized.
		private bool seekPreview = false;           //  To preview first frame of seeking when seek under paused state.
		private Texture2D videoTexYch = null;
		private Texture2D videoTexUch = null;
		private Texture2D videoTexVch = null;
		private int videoWidth	= -1;
		private int videoHeight	= -1;
		
		private const int AUDIO_FRAME_SIZE = 2048;  //  Audio clip data size. Packed from audioDataBuff.
        private const int SWAP_BUFFER_NUM = 4;		//	How many audio source to swap.
		private AudioSource[] audioSource = new AudioSource[SWAP_BUFFER_NUM];
		private List<float> audioDataBuff = null;   //  Buffer to keep audio data decoded from native.
		public int audioFrequency { get; private set; }
        public int audioChannels { get; private set; }
        private const double OVERLAP_TIME = 0.02;   //  Our audio clip is defined as: [overlay][audio data][overlap].
        private int audioOverlapLength = 0;         //  OVERLAP_TIME * audioFrequency.
        private int audioDataLength = 0;            //  (AUDIO_FRAME_SIZE + 2 * audioOverlapLength) * audioChannel.
        private float volume = 1.0f;

        //	Time control
        private double globalStartTime  = 0;        //  Video and audio progress are based on this start time.
		private bool isVideoReadyToReplay = false;
		private bool isAudioReadyToReplay = false;
		private double audioProgressTime = -1.0;
		private double hangTime = -1.0f;            //  Used to set progress time after seek/resume.
        private double firstAudioFrameTime = -1.0;
        public float videoTotalTime { get; private set; }   //  Video duration.
        public float audioTotalTime { get; private set; }   //  Audio duration.

        private BackgroundWorker backgroundWorker;
        private object _lock = new object();

        void Awake () {
			print(LOG_TAG + " ver." + VERSION);
			if (playOnAwake) {
				print (LOG_TAG + " play on wake.");
				onInitComplete.AddListener(startDecoding);
				initDecoder(mediaPath);
			}
		}

        //  Video progress is triggered using Update. Progress time would be set by nativeSetVideoTime.
        void Update() {
            switch (decoderState) {
				case DecoderState.START:
					if (isVideoEnabled) {
						//  Prevent empty texture generate green screen.(default 0,0,0 in YUV which is green in RGB)
						if (useDefault && nativeIsContentReady(decoderID)) {
							getTextureFromNative();
							setTextures(videoTexYch, videoTexUch, videoTexVch);
							useDefault = false;
						}

						//	Update video frame by dspTime.
						double setTime = AudioSettings.dspTime - globalStartTime;

						//	Normal update frame.
						if (setTime < videoTotalTime || videoTotalTime == -1.0f) {
							if (seekPreview && nativeIsContentReady(decoderID)) {
								setPause();
								seekPreview = false;
								unmute();
							} else {
								nativeSetVideoTime(decoderID, (float) setTime);
								GL.IssuePluginEvent(GetRenderEventFunc(), decoderID);
							}
						} else {
							isVideoReadyToReplay = true;
						}
					}

                    if (nativeIsVideoBufferEmpty(decoderID) && !nativeIsEOF(decoderID)) {
                        decoderState = DecoderState.BUFFERING;
                        hangTime = AudioSettings.dspTime - globalStartTime;
                    }

                    break;

				case DecoderState.SEEK_FRAME:
					if (nativeIsSeekOver(decoderID)) {
						globalStartTime = AudioSettings.dspTime - hangTime;
						decoderState = DecoderState.START;
						if (lastState == DecoderState.PAUSE) {
							seekPreview = true;
							mute();
						}
					}
					break;

                case DecoderState.BUFFERING:
                    if (nativeIsVideoBufferFull(decoderID) || nativeIsEOF(decoderID)) {
                        decoderState = DecoderState.START;
                        globalStartTime = AudioSettings.dspTime - hangTime;
                    }
                    break;

                case DecoderState.PAUSE:
                case DecoderState.EOF:
                default:
					break;
			}

			if (isVideoEnabled || isAudioEnabled) {
				if ((!isVideoEnabled || isVideoReadyToReplay) && (!isAudioEnabled || isAudioReadyToReplay)) {
                    decoderState = DecoderState.EOF;
                    isVideoReadyToReplay = isAudioReadyToReplay = false;

                    if (onVideoEnd != null) {
						onVideoEnd.Invoke();
					}
                }
			}
		}
        
        public void initDecoder(string path, bool enableAllAudioCh = false) {
            isAllAudioChEnabled = enableAllAudioCh;
            StartCoroutine(initDecoderAsync(path));
        }
        
		IEnumerator initDecoderAsync(string path) {
			print(LOG_TAG + " init Decoder.");
			decoderState = DecoderState.INITIALIZING;

			mediaPath = path;
			decoderID = -1;
			nativeCreateDecoderAsync(mediaPath, ref decoderID);

			int result = 0;
			do {
				yield return null;
				result = nativeGetDecoderState(decoderID);
			} while (!(result == 1 || result == -1));

			//  Init success.
			if (result == 1) {
				print(LOG_TAG + " Init success.");
				isVideoEnabled = nativeIsVideoEnabled(decoderID);
				if (isVideoEnabled) {
					float duration = 0.0f;
					nativeGetVideoFormat(decoderID, ref videoWidth, ref videoHeight, ref duration);
					videoTotalTime = duration > 0 ? duration : -1.0f ;
					print(LOG_TAG + " Video format: (" + videoWidth + ", " + videoHeight + ")");
					print(LOG_TAG + " Total time: " + videoTotalTime);

					setTextures(null, null, null);
					useDefault = true;
				}

				//	Initialize audio.
				isAudioEnabled = nativeIsAudioEnabled(decoderID);
				print(LOG_TAG + " isAudioEnabled = " + isAudioEnabled);
				if (isAudioEnabled) {
                    if (isAllAudioChEnabled) {
                        nativeSetAudioAllChDataEnable(decoderID, isAllAudioChEnabled);
                        getAudioFormat();
                    } else {
                        getAudioFormat();
                        initAudioSource();
                    }
                }

				decoderState = DecoderState.INITIALIZED;

				if (onInitComplete != null) {
					onInitComplete.Invoke();
				}
			} else {
				print(LOG_TAG + " Init fail.");
				decoderState = DecoderState.INIT_FAIL;
			}
		}

        private void getAudioFormat() {
            int channels = 0;
            int freqency = 0;
            float duration = 0.0f;
            nativeGetAudioFormat(decoderID, ref channels, ref freqency, ref duration);
            audioChannels = channels;
            audioFrequency = freqency;
            audioTotalTime = duration > 0 ? duration : -1.0f;
            print(LOG_TAG + " audioChannel " + audioChannels);
            print(LOG_TAG + " audioFrequency " + audioFrequency);
            print(LOG_TAG + " audioTotalTime " + audioTotalTime);
        }

        private void initAudioSource() {
            getAudioFormat();
            audioOverlapLength = (int) (OVERLAP_TIME * audioFrequency + 0.5f);

            audioDataLength = (AUDIO_FRAME_SIZE + 2 * audioOverlapLength) * audioChannels;
            for (int i = 0; i < SWAP_BUFFER_NUM; i++) {
                if (audioSource[i] == null) {
                    audioSource[i] = gameObject.AddComponent<AudioSource>();
                }
                audioSource[i].clip = AudioClip.Create("testSound" + i, audioDataLength, audioChannels, audioFrequency, false);
                audioSource[i].playOnAwake = false;
                audioSource[i].volume = volume;
                audioSource[i].minDistance = audioSource[i].maxDistance;
            }
        }
		
		public void startDecoding() {
			if(decoderState == DecoderState.INITIALIZED) {
                if (!nativeStartDecoding(decoderID)) {
					print (LOG_TAG + " Decoding not start.");
					return;
				}

                decoderState = DecoderState.BUFFERING;
                globalStartTime = AudioSettings.dspTime;
                hangTime = AudioSettings.dspTime - globalStartTime;

                isVideoReadyToReplay = isAudioReadyToReplay = false;
				if(isAudioEnabled && !isAllAudioChEnabled) {
                    StartCoroutine ("audioPlay");
                    backgroundWorker = new BackgroundWorker();
                    backgroundWorker.WorkerSupportsCancellation = true;
                    backgroundWorker.DoWork += new DoWorkEventHandler(pullAudioData);
                    backgroundWorker.RunWorkerAsync();
                }
			}
		}

        private void pullAudioData(object sender, DoWorkEventArgs e) {
            IntPtr dataPtr = IntPtr.Zero;      //	Pointer to get audio data from native.
            float[] tempBuff = new float[0];    //	Buffer to copy audio data from dataPtr to audioDataBuff.
            int audioFrameLength = 0;
            double lastTime = -1.0f;            //	Avoid to schedule the same audio data set.

            audioDataBuff = new List<float>();
            while (decoderState >= DecoderState.START) {
                if (decoderState != DecoderState.SEEK_FRAME) {
                    double audioNativeTime = nativeGetAudioData(decoderID, ref dataPtr, ref audioFrameLength);
                    if (0 < audioNativeTime && lastTime != audioNativeTime && decoderState != DecoderState.SEEK_FRAME && audioFrameLength != 0) {
                        if (firstAudioFrameTime == -1.0) {
                            firstAudioFrameTime = audioNativeTime;
                        }

                        lastTime = audioNativeTime;
                        audioFrameLength *= audioChannels;
                        if (tempBuff.Length != audioFrameLength) {
                            //  For dynamic audio data length, reallocate the memory if needed.
                            tempBuff = new float[audioFrameLength];
                        }
                        Marshal.Copy(dataPtr, tempBuff, 0, audioFrameLength);
                        lock (_lock) {
                            audioDataBuff.AddRange(tempBuff);
                        }
                    }

                    if (audioNativeTime != -1.0) {
                        nativeFreeAudioData(decoderID);
                    }

                    System.Threading.Thread.Sleep(2);
                }
            }
            
            lock (_lock) {
                audioDataBuff.Clear();
                audioDataBuff = null;
            }
        }

		private void getTextureFromNative() {
			ReleaseTexture();

			IntPtr nativeTexturePtrY = new IntPtr();
			IntPtr nativeTexturePtrU = new IntPtr();
			IntPtr nativeTexturePtrV = new IntPtr();
			nativeCreateTexture(decoderID, ref nativeTexturePtrY, ref nativeTexturePtrU, ref nativeTexturePtrV);
			videoTexYch = Texture2D.CreateExternalTexture(
				videoWidth, videoHeight, TextureFormat.Alpha8, false, false, nativeTexturePtrY);
			videoTexUch = Texture2D.CreateExternalTexture(
				videoWidth / 2, videoHeight / 2, TextureFormat.Alpha8, false, false, nativeTexturePtrU);
			videoTexVch = Texture2D.CreateExternalTexture(
				videoWidth / 2, videoHeight / 2, TextureFormat.Alpha8, false, false, nativeTexturePtrV);
		}

		private void ReleaseTexture() {
			setTextures(null, null, null);
			
			videoTexYch = null;
			videoTexUch = null;
			videoTexVch = null;

			useDefault = true;
		}

		private void setTextures(Texture ytex, Texture utex, Texture vtex) {
			Material texMaterial = GetComponent<MeshRenderer>().material;
			texMaterial.SetTexture("_YTex", ytex);
			texMaterial.SetTexture("_UTex", utex);
			texMaterial.SetTexture("_VTex", vtex);
		}

		public void replay() {
            if (setSeekTime(0.0f)) {
                globalStartTime = AudioSettings.dspTime;
                isVideoReadyToReplay = isAudioReadyToReplay = false;
            }
		}

        public void getAllAudioChannelData(out float[] data, out double time, out int samplesPerChannel) {
            if (!isAllAudioChEnabled) {
                print(LOG_TAG + " this function only works for isAllAudioEnabled == true.");
                data = null;
                time = 0;
                samplesPerChannel = 0;
                return;
            }

            IntPtr dataPtr = new IntPtr();
            int lengthPerChannel = 0;
            double audioNativeTime = nativeGetAudioData(decoderID, ref dataPtr, ref lengthPerChannel);
            float[] buff = null;
            if (lengthPerChannel > 0) {
                buff = new float[lengthPerChannel * audioChannels];
                Marshal.Copy(dataPtr, buff, 0, buff.Length);
                nativeFreeAudioData(decoderID);
            }

            data = buff;
            time = audioNativeTime;
            samplesPerChannel = lengthPerChannel;
        }

		IEnumerator audioPlay() {
			print (LOG_TAG + " start audio play coroutine.");
			int swapIndex = 0;                  //	Swap between audio sources.
			double audioDataTime = (double) AUDIO_FRAME_SIZE / audioFrequency;
            int playedAudioDataLength = AUDIO_FRAME_SIZE * audioChannels; //  Data length exclude the overlap length.

            print(LOG_TAG + " audioDataTime " + audioDataTime);

            audioProgressTime = -1.0;           //  Used to schedule each audio clip to be played.
			while(decoderState >= DecoderState.START) {
				if(decoderState == DecoderState.START) {
					double currentTime = AudioSettings.dspTime - globalStartTime;
					if(currentTime < audioTotalTime || audioTotalTime == -1.0f) {
                        if (audioDataBuff != null && audioDataBuff.Count >= audioDataLength) {
                            if (audioProgressTime == -1.0) {
                                //  To simplify, the first overlap data would not be played.
                                //  Correct the audio progress time by adding OVERLAP_TIME.
                                audioProgressTime = firstAudioFrameTime + OVERLAP_TIME;
                                globalStartTime = AudioSettings.dspTime - audioProgressTime;
                            }

                            while (audioSource[swapIndex].isPlaying || decoderState == DecoderState.SEEK_FRAME) { yield return null; }

                            //  Re-check data length if audioDataBuff is cleared by seek.
                            if (audioDataBuff.Count >= audioDataLength) {
                                double playTime = audioProgressTime + globalStartTime;
                                double endTime = playTime + audioDataTime;

                                //  If audio is late, adjust start time and re-calculate audio clip time.
                                if (playTime <= AudioSettings.dspTime) {
                                    globalStartTime = AudioSettings.dspTime - audioProgressTime;
                                    playTime = audioProgressTime + globalStartTime;
                                    endTime = playTime + audioDataTime;
                                }

                                audioSource[swapIndex].clip.SetData(audioDataBuff.GetRange(0, audioDataLength).ToArray(), 0);
                                audioSource[swapIndex].PlayScheduled(playTime);
                                audioSource[swapIndex].SetScheduledEndTime(endTime);
                                audioSource[swapIndex].time = (float) OVERLAP_TIME;
                                audioProgressTime += audioDataTime;
                                swapIndex = (swapIndex + 1) % SWAP_BUFFER_NUM;

                                lock (_lock) {
                                    audioDataBuff.RemoveRange(0, playedAudioDataLength);
                                }
                            }
                        }
					} else {
						//print(LOG_TAG + " Audio reach EOF. Prepare replay.");
						isAudioReadyToReplay = true;
						audioProgressTime = firstAudioFrameTime = - 1.0;
                        if (audioDataBuff != null) {
                            lock (_lock) {
                                audioDataBuff.Clear();
                            }
                        }
                    }
				}
				yield return new WaitForFixedUpdate();
			}
        }

		public void stopDecoding() {
			if(decoderState >= DecoderState.INITIALIZING) {
				print (LOG_TAG + " stop decoding.");
				decoderState = DecoderState.STOP;
				ReleaseTexture();
				if (isAudioEnabled) {
					StopCoroutine ("audioPlay");
                    backgroundWorker.CancelAsync();

                    if (audioSource != null) {
						for (int i = 0; i < SWAP_BUFFER_NUM; i++) {
							if (audioSource[i] != null) {
								AudioClip.Destroy(audioSource[i].clip);
								AudioSource.Destroy(audioSource[i]);
								audioSource[i] = null;
							}
						}
					}
				}

				nativeDestroyDecoder (decoderID);
				decoderID = -1;
				decoderState = DecoderState.NOT_INITIALIZED;

				isVideoEnabled = isAudioEnabled = false;
				isVideoReadyToReplay = isAudioReadyToReplay = false;
                isAllAudioChEnabled = false;
            }
		}

		public bool setSeekTime(float seekTime) {
            if (decoderState != DecoderState.SEEK_FRAME && decoderState >= DecoderState.START) {
                lastState = decoderState;
                decoderState = DecoderState.SEEK_FRAME;

                float setTime = 0.0f;
                if ((isVideoEnabled && seekTime > videoTotalTime) ||
                    (isAudioEnabled && seekTime > audioTotalTime) ||
                    isVideoReadyToReplay || isAudioReadyToReplay ||
                    seekTime < 0.0f) {
                    print(LOG_TAG + " Seek over end. ");
                    setTime = 0.0f;
                } else {
                    setTime = seekTime;
                }
                
                print(LOG_TAG + " set seek time: " + setTime);
                hangTime = setTime;
                nativeSetSeekTime(decoderID, setTime);
                nativeSetVideoTime(decoderID, (float) setTime);

                if (isAudioEnabled) {
                    lock (_lock) {
	                    audioDataBuff.Clear();
                    }
                    audioProgressTime = firstAudioFrameTime = -1.0;
                    foreach (AudioSource src in audioSource) {
                        src.Stop();
                    }
                }

                return true;
            } else {
                return false;
            }
		}
		
		public bool isSeeking() {
			return decoderState >= DecoderState.INITIALIZED && (decoderState == DecoderState.SEEK_FRAME || !nativeIsContentReady(decoderID));
		}

        public bool isVideoEOF() {
            return decoderState == DecoderState.EOF;
        }

		public void setStepForward(float sec) {
			double targetTime = AudioSettings.dspTime - globalStartTime + sec;
            if (setSeekTime((float) targetTime)) {
                print(LOG_TAG + " set forward : " + sec);
            }
		}

		public void setStepBackward(float sec) {
			double targetTime = AudioSettings.dspTime - globalStartTime - sec;
            if (setSeekTime((float) targetTime)) {
                print(LOG_TAG + " set backward : " + sec);
            }
		}

		public void getVideoResolution(ref int width, ref int height) {
			width = videoWidth;
			height = videoHeight;
		}

		public float getVideoCurrentTime() {
			if (decoderState == DecoderState.PAUSE || decoderState == DecoderState.SEEK_FRAME) {
				return (float) hangTime;
			} else {
				return (float) (AudioSettings.dspTime - globalStartTime);
			}
		}

		public DecoderState getDecoderState() {
			return decoderState;
		}

		public void setPause() {
			if (decoderState == DecoderState.START) {
				hangTime = AudioSettings.dspTime - globalStartTime;
				decoderState = DecoderState.PAUSE;
				if (isAudioEnabled) {
					foreach (AudioSource src in audioSource) {
						src.Pause();
					}
				}
			}
		}

		public void setResume() {
			if (decoderState == DecoderState.PAUSE) {
				globalStartTime = AudioSettings.dspTime - hangTime;
				decoderState = DecoderState.START;
				if (isAudioEnabled) {
					foreach (AudioSource src in audioSource) {
						src.UnPause();
					}
				}
			}
		}

		public void setVolume(float vol) {
			volume = Mathf.Clamp(vol, 0.0f, 1.0f);
			foreach (AudioSource src in audioSource) {
				if(src != null) {
					src.volume = volume;
				}
			}
		}

		public float getVolume() {
			return volume;
		}

		public void mute() {
			float temp = volume;
			setVolume(0.0f);
			volume = temp;
		}

		public void unmute() {
			setVolume(volume);
		}

		public static void getMetaData(string filePath, out string[] key, out string[] value) {
			IntPtr keyptr = IntPtr.Zero;
			IntPtr valptr = IntPtr.Zero;

			int metaCount = nativeGetMetaData(filePath, out keyptr, out valptr);

			IntPtr[] keys = new IntPtr[metaCount];
			IntPtr[] vals = new IntPtr[metaCount];
			Marshal.Copy(keyptr, keys, 0, metaCount);
			Marshal.Copy(valptr, vals, 0, metaCount);

			string[] keyArray = new string[metaCount];
			string[] valArray = new string[metaCount];
			for (int i = 0; i < metaCount; i++) {
				keyArray[i] = Marshal.PtrToStringAnsi(keys[i]);
				valArray[i] = Marshal.PtrToStringAnsi(vals[i]);
				Marshal.FreeCoTaskMem(keys[i]);
				Marshal.FreeCoTaskMem(vals[i]);
			}
			Marshal.FreeCoTaskMem(keyptr);
			Marshal.FreeCoTaskMem(valptr);

			key = keyArray;
			value = valArray;
		}

        public static void loadVideoThumb(GameObject obj, string filePath, float time)
        {
            if (!System.IO.File.Exists(filePath)) {
                print(LOG_TAG + " File not found!");
                return;
            }

            int decID = -1;
            int width = 0;
            int height = 0;
            float totalTime = 0.0f;
            nativeCreateDecoder(filePath, ref decID);
            nativeGetVideoFormat(decID, ref width, ref height, ref totalTime);
            if (!nativeStartDecoding(decID)) {
                print(LOG_TAG + " Decoding not start.");
                return;
            }

            Texture2D thumbY = new Texture2D(width, height, TextureFormat.Alpha8, false);
            Texture2D thumbU = new Texture2D(width / 2, height / 2, TextureFormat.Alpha8, false);
            Texture2D thumbV = new Texture2D(width / 2, height / 2, TextureFormat.Alpha8, false);
            Material thumbMat = obj.GetComponent<MeshRenderer>().material;
            if (thumbMat == null) {
                print(LOG_TAG + " Target has no MeshRenderer.");
                nativeDestroyDecoder(decID);
                return;
            }
            thumbMat.SetTexture("_YTex", thumbY);
            thumbMat.SetTexture("_UTex", thumbU);
            thumbMat.SetTexture("_VTex", thumbV);
            
            nativeLoadThumbnail(decID, time, thumbY.GetNativeTexturePtr(), thumbU.GetNativeTexturePtr(), thumbV.GetNativeTexturePtr());
            nativeDestroyDecoder(decID);
        }

        public void setAudioEnable(bool isEnable) {
            nativeSetAudioEnable(decoderID, isEnable);
            if (isEnable) {
                setSeekTime(getVideoCurrentTime());
            }
        }

        public void setVideoEnable(bool isEnable) {
            nativeSetVideoEnable(decoderID, isEnable);
            if (isEnable) {
                setSeekTime(getVideoCurrentTime());
            }
        }

        void OnApplicationQuit() {
			print (LOG_TAG + " OnApplicationQuit");
			stopDecoding ();
		}

		void OnDestroy() {
			print (LOG_TAG + " OnDestroy");
			stopDecoding ();
		}
	}
}