//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

using UnityEngine;
using System.IO;
using System.Collections;
using System.Collections.Generic;

namespace HTC.UnityPlugin.Multimedia
{
	public class FileSeeker {
		private const string LOG_TAG = "[FileSeeker]";

		private List<FileInfo> contentInfo = new List<FileInfo>();
		public int contentIndex { get; private set; }

		public bool loadFolder(string path, string filter) {
			if (!Directory.Exists(path)) {
				Debug.Log(LOG_TAG + " path invalid.");
				return false;
			}

			string[] multipleFilter = filter.Split(new char[] { '|' });
			DirectoryInfo dir = new DirectoryInfo(path);
			contentInfo.Clear();
			for (int i = 0; i < multipleFilter.Length; i++) {
				contentInfo.AddRange(dir.GetFiles(multipleFilter[i]));
			}

			dir = null;
			Debug.Log(LOG_TAG + " Found " + contentInfo.Count + " files.");
			return contentInfo.Count > 0;
		}

		public string getPath() {
			if (contentInfo.Count > 0) {
				string path = contentInfo[contentIndex].FullName;
				Debug.Log(LOG_TAG + " Get path " + path);
				return path;
			} else {
				Debug.Log(LOG_TAG + " No path");
				return "";
			}
		}

		public string[] getPathAll() {
			string[] pathList = new string[contentInfo.Count];
			for (int i = 0; i < contentInfo.Count; i++) {
				pathList[i] = contentInfo[i].FullName;
			}

			return pathList;
		}

		public int getFileNum() {
			return contentInfo.Count;
		}

		//  Index control
		public int toNext() {
			if (contentInfo.Count > 0) {
				contentIndex = (contentIndex + 1) % contentInfo.Count;
				Debug.Log(LOG_TAG + " To next file index:" + contentIndex);
			}

            return contentIndex;
		}

		public int toPrev() {
			if (contentInfo.Count > 0) {
				contentIndex = (contentIndex + contentInfo.Count - 1) % contentInfo.Count;
				Debug.Log(LOG_TAG + " To prev file index:" + contentIndex);
			}

            return contentIndex;
        }

		public void setIndex(int index) {
			if (contentInfo.Count > 0) {
				contentIndex = index % contentInfo.Count;
				Debug.Log(LOG_TAG + " To file index:" + contentIndex);
			}
		}
	}
}