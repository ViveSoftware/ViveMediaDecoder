//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;

namespace HTC.UnityPlugin.Multimedia
{
	[System.Serializable]
	public class StereoProperty {
		public bool isLeftFirst;
		public StereoHandler.StereoType stereoType = StereoHandler.StereoType.TOP_DOWN;

		public GameObject left;
		public GameObject right;
	}
}