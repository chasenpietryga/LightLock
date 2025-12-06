// Add this to LightLockUnity.cs (only new part)
#if UNITY_EDITOR
[UnityEditor.MenuItem("Tools/LightLock/Rebake Static Scene")]
static void RebakeScene() {
    string baker = EditorApplication.applicationContentsPath;
    if (Application.platform == RuntimePlatform.OSXEditor)   baker += "/Tools/LightLockBaker";
    else if (Application.platform == RuntimePlatform.WindowsEditor) baker = "C:/LightLock/LightLockBaker.exe";
    else { Debug.LogError("No baker for this platform"); return; }

    var path = EditorUtility.SaveFilePanel("Save lightlock.bin", "", "lightlock.bin", "");
    if (string.IsNullOrEmpty(path)) return;

    var proc = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo {
        FileName = baker,
        Arguments = $"\"{SceneManager.GetActiveScene().path.Replace(".unity", ".fbx")}\" \"{path}\"",
        UseShellExecute = false,
        RedirectStandardOutput = true
    });
    proc.WaitForExit();
    Debug.Log($"[LightLock] Re-baked → {path}");
}
#endif
