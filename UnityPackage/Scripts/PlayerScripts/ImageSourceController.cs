//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;
using UnityEngine.Events;
using System.Collections;

namespace HTC.UnityPlugin.Multimedia
{
	[RequireComponent(typeof(MeshRenderer))]
	public class ImageSourceController : MonoBehaviour {
		protected string LOG_TAG = "[ImageSourceController]";

		public string folderPath;
		public string filter;
		public bool isAdaptToResolution;
		public UnityEvent onInitComplete;
		public UnityEvent onChangeImage;
		protected bool isInitialized = false;
		protected FileSeeker fileSeeker;

		protected Texture2D texture;
		protected Vector3 oriScale;

		protected virtual void Start() {
			initFileSeeker();
			texture = new Texture2D(1, 1);
			texture.filterMode = FilterMode.Trilinear;
			texture.Apply();
			GetComponent<MeshRenderer>().material.mainTexture = texture;
		}

		public void initFileSeeker() {
			if (folderPath == null) {
				Debug.Log(LOG_TAG + "Folder path is null.");
				return;
			}

			isInitialized = false;

			fileSeeker = new FileSeeker();
			if (!fileSeeker.loadFolder(folderPath, filter)) {
				Debug.Log(LOG_TAG + " content not found.");
				fileSeeker = null;
				return;
			}

			oriScale = transform.localScale;

			isInitialized = true;

			onInitComplete.Invoke();
		}

		public void loadImage() {
			if (!isInitialized) {
				Debug.Log(LOG_TAG + " not initialized.");
				return;
			}
			
			StartCoroutine(loadImageCoroutine(fileSeeker.getPath()));
		}

		public void nextImage() {
			if (!isInitialized) {
				Debug.Log(LOG_TAG + " not initialized.");
				return;
			}

			fileSeeker.toNext();

			onChangeImage.Invoke();
		}

		public void prevImage() {
			if (!isInitialized) {
				Debug.Log(LOG_TAG + " not initialized.");
				return;
			}

			fileSeeker.toPrev();

			onChangeImage.Invoke();
		}

		protected IEnumerator loadImageCoroutine(string imagePath) {
			var www = new WWW("file://" + imagePath);
			yield return www;
			www.LoadImageIntoTexture(texture);

			if (isAdaptToResolution) {
				adaptResolution();
			}
		}

		protected virtual void adaptResolution() {
			int width = texture.width;
			int height = texture.height;
			Vector3 adaptReso = oriScale;
			adaptReso.x *= ((float) width / height);
			transform.localScale = adaptReso;
		}
	}
}