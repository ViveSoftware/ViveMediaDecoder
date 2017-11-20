//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;
using System.Collections;

namespace HTC.UnityPlugin.Multimedia
{
	public class StereoVideoSourceController : VideoSourceController {
		public StereoProperty stereoProperty;

		// Use this for initialization
		protected override void Start () {
			LOG_TAG = "[StereoVideoSourceController]";
			base.Start();
			StereoHandler.SetStereoPair(stereoProperty, GetComponent<MeshRenderer>().material);
		}

		protected override void adaptResolution() {
			int width = 1;
			int height = 1;
			decoder.getVideoResolution(ref width, ref height);
			if (stereoProperty.stereoType == StereoHandler.StereoType.TOP_DOWN) {
				height /= 2;
			} else {
				width /= 2;
			}
			Vector3 adaptReso = oriScale;
			adaptReso.x *= ((float) width / height);
			transform.localScale = adaptReso;
		}
	}
}