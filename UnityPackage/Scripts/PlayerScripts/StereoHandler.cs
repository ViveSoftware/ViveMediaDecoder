//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;
using System.Collections;

namespace HTC.UnityPlugin.Multimedia
{
	public static class StereoHandler {
		//	Stereo members
		public enum StereoType {SIDE_BY_SIDE, TOP_DOWN};
		public static void SetStereoPair(StereoProperty stereoProperty, Material material) {
			SetStereoPair(stereoProperty.left, stereoProperty.right, material, stereoProperty.stereoType, stereoProperty.isLeftFirst);
		}
		public static void SetStereoPair(GameObject left, GameObject right, Material material, StereoType stereoType, bool isLeftFirst) {
			if (left == null || right == null) {
				Debug.Log("Stereo targets are null.");
				return;
			}

			//  0.Get left frame and right frame UV
			MeshFilter leftMesh = left.GetComponent<MeshFilter>();
			MeshFilter rightMesh = right.GetComponent<MeshFilter>();

			if (leftMesh != null && rightMesh != null) {
				Vector2[] leftUV = leftMesh.mesh.uv;
				Vector2[] rightUV = rightMesh.mesh.uv;

				//  1.Modify UV
				for (int i = 0; i < leftUV.Length; i++) {
					if (stereoType == StereoType.SIDE_BY_SIDE) {
						leftUV[i].x = leftUV[i].x / 2;
						rightUV[i].x = rightUV[i].x / 2;
						if (isLeftFirst) {
							rightUV[i].x += 0.5f;
						} else {
							leftUV[i].x += 0.5f;
						}
					} else if (stereoType == StereoType.TOP_DOWN) {
						leftUV[i].y = leftUV[i].y / 2;
						rightUV[i].y = rightUV[i].y / 2;
						if (!isLeftFirst) { //  For inverse uv, the order is inversed too.
							rightUV[i].y += 0.5f;
						} else {
							leftUV[i].y += 0.5f;
						}
					}
				}

				leftMesh.mesh.uv = leftUV;
				rightMesh.mesh.uv = rightUV;
			}

			//  2.Assign texture
			left.GetComponent<MeshRenderer>().material = material;
			right.GetComponent<MeshRenderer>().material = material;
		}
	}
}