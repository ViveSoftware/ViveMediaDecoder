//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;
using System.Collections;

namespace HTC.UnityPlugin.Multimedia
{
	[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
	public class UVSphere : MonoBehaviour {
		public enum FrontFaceType { Inside, Outside };
		public enum TextureOrigin { BottomLeft, TopLeft };
		public int Latitude = 32;
		public int Longitude = 32;
		public float Radius = 1.0f;
		public FrontFaceType frontFaceType = FrontFaceType.Outside;
		public TextureOrigin textureOrigin = TextureOrigin.BottomLeft;

		// Use this for initialization
		void Awake () {
			GenerateUVSphere(Latitude, Longitude, Radius);
		}
		
		//	This function is amid to generate a UV sphere which top and bottom is multiple vertices.
		void GenerateUVSphere(int latNum, int longNum, float radius) {
			int vertexNum   = (longNum + 1) * (latNum + 2);
			int meshNum     = (longNum) * (latNum + 1);
			int triangleNum = meshNum * 2;

			//	1.Calculate vertices
			Vector3[] vertices = new Vector3[vertexNum];
			float PI = Mathf.PI;
			float PI2 = PI * 2.0f;
			int latIdxMax = latNum + 1;     //  Latitude vertex number is latNum + 2, index numbers are from 0 ~ latNum + 1
			int longVertNum = longNum + 1;  //  Longitude vertex number is longNum + 1, index numbers are from 0 ~ longNum
			float preComputeV = PI / latIdxMax;
			float preComputeH = PI2 / longNum;
			for (int i = 0; i <= latIdxMax; i++) {
				float thetaV = i * preComputeV;     //  PI * i / latIdxMax;
				float sinV = Mathf.Sin(thetaV);
				float cosV = Mathf.Cos(thetaV);
				int lineStartIdx = i * longVertNum;
				for (int j = 0; j <= longNum; j++) {
					float thetaH = j * preComputeH; //  PI2 * j / longNum;
					vertices[lineStartIdx + j] = new Vector3(
						Mathf.Cos(thetaH) * sinV,
						cosV,
						Mathf.Sin(thetaH) * sinV
					) * radius;
				}
			}

			//	2.Calculate normals
			Vector3[] normals = new Vector3[vertices.Length];
			for (int i = 0; i < vertices.Length; i++) {
				normals[i] = vertices[i].normalized;
				if (frontFaceType == FrontFaceType.Inside) {
					normals[i] *= -1.0f;
				}
			}

			//	3.Calculate uvs
			Vector2[] uvs = new Vector2[vertices.Length];
			for (int i = 0; i <= latIdxMax; i++) {
				int lineStartIdx = i * longVertNum;
				float vVal = (float) i / latIdxMax;
				if (textureOrigin == TextureOrigin.BottomLeft) {
					vVal = 1.0f - vVal;
				}
				for (int j = 0; j <= longNum; j++) {
					float uVal = (float) j / longNum;
					if (frontFaceType == FrontFaceType.Inside) {
						uVal = 1.0f - uVal;
					}
					uvs[lineStartIdx + j] = new Vector2(uVal, vVal);
				}
			}

			//	4.Calculate triangles
			int[] triangles = new int[triangleNum * 3];
			int index = 0;
			for (int i = 0; i <= latNum; i++) {
				for (int j = 0; j < longNum; j++) {
					int curVertIdx = i * longVertNum + j;
					int nextLineVertIdx = curVertIdx + longVertNum;

					if (frontFaceType == FrontFaceType.Outside) {
						triangles[index++] = curVertIdx;
						triangles[index++] = curVertIdx + 1;
						triangles[index++] = nextLineVertIdx + 1;
						triangles[index++] = curVertIdx;
						triangles[index++] = nextLineVertIdx + 1;
						triangles[index++] = nextLineVertIdx;
					} else {
						triangles[index++] = curVertIdx;
						triangles[index++] = nextLineVertIdx + 1;
						triangles[index++] = curVertIdx + 1;
						triangles[index++] = curVertIdx;
						triangles[index++] = nextLineVertIdx;
						triangles[index++] = nextLineVertIdx + 1;
					}
				}
			}

			//	5.Assign to mesh
			MeshFilter filter = gameObject.GetComponent<MeshFilter>();
			Mesh mesh = filter.mesh;
			mesh.Clear();
			mesh.vertices = vertices;
			mesh.normals = normals;
			mesh.uv = uvs;
			mesh.triangles = triangles;
			;
			MeshRenderer renderer = gameObject.GetComponent<MeshRenderer>();
			renderer.material.mainTexture = Texture2D.blackTexture;
		}
	}
}