using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.HighDefinition;
using System.Runtime.InteropServices;
using System.IO;

// ================================================================
// P/Invoke to our 112-line pruned C++ core
// ================================================================
public static class LightLockNative {
    const string DLL = "LightLock";
    [DllImport(DLL)] public static extern bool hit(uint hash, float[] color, ref float weight);
    [DllImport(DLL)] public static extern void store(uint hash, float[] color, float weight, bool permanent);
    [DllImport(DLL)] public static extern void miss_and_writeback(uint hash, float[] color, float weight);
    [DllImport(DLL)] public static extern void invalidate_sector(ref Vector3 min, ref Vector3 max);
    [DllImport(DLL)] public static extern void flush();
}

// ================================================================
// One-component does everything – drop on any GameObject
// ================================================================
[ExecuteAlways]
public class LightLockUnity : MonoBehaviour {
    [Header("Auto-bake on play (editor & build)")]
    public bool autoBakeStatic = true;

    static LightLockUnity instance;

    void Awake() {
        if (instance == null) instance = this;
        else if (instance != this) Destroy(this);
    }

    // ----------------------------------------------------------------
    // Public static API – call from your ray tracers / probes
    // ----------------------------------------------------------------
    public static bool Hit(uint hash, out Vector3 color, out float weight) {
        color = Vector3.zero; weight = 0f;
        float[] c = new float[3];
        bool hit = LightLockNative.hit(hash, c, ref weight);
        if (hit) color = new Vector3(c[0], c[1], c[2]);
        return hit;
    }

    public static void Cache(uint hash, Vector3 color, float weight = 1f, bool permanent = false) {
        float[] c = { color.x, color.y, color.z };
        if (permanent)
            LightLockNative.store(hash, c, weight, true);
        else
            LightLockNative.miss_and_writeback(hash, c, weight);
    }

    public static void InvalidateBox(Bounds bounds) {
        Vector3 min = bounds.min;
        Vector3 max = bounds.max;
        LightLockNative.invalidate_sector(ref min, ref max);
    }

    // ----------------------------------------------------------------
    // Automatic static baker – runs once per scene in editor & build
    // ----------------------------------------------------------------
#if UNITY_EDITOR
    [UnityEditor.InitializeOnLoadMethod]
    static void EditorInit() {
        UnityEditor.EditorApplication.playModeStateChanged += state => {
            if (state == UnityEditor.PlayModeStateChange.EnteredPlayMode && instance && instance.autoBakeStatic)
                instance.BakeStaticScene();
        };
    }
#endif

    void BakeStaticScene() {
        if (!autoBakeStatic) return;
        Debug.Log("[LightLock] Baking static geometry...");
        // Simple brute-force baker – replace with our full C++ baker later
        foreach (var renderer in FindObjectsOfType<MeshRenderer>()) {
            if (!renderer.gameObject.isStatic) continue;
            var mesh = renderer.GetComponent<MeshFilter>().sharedMesh;
            uint hash = (uint)mesh.GetInstanceID(); // good enough for static
            Vector3 col = new Vector3(0.7f, 0.7f, 0.8f); // fake diffuse for demo
            Cache(hash, col, 1f, permanent: true);
        }
        LightLockNative.flush();
        Debug.Log($"[LightLock] Baked {FindObjectsOfType<MeshRenderer>(true).Length} static objects");
    }
}

// ================================================================
// Example usage in any ray-tracing shader or script
// ================================================================
public class ExampleRayProbe : MonoBehaviour {
    uint GetHash() => (uint)System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(gameObject);

    void Update() {
        uint hash = GetHash();
        if (LightLockUnity.Hit(hash, out Vector3 col, out float w)) {
            // Cache hit – instant!
            RenderSettings.ambientLight = col * w;
        } else {
            // Cache miss – do expensive thing once
            Vector3 realColor = ExpensiveTrace(); // your real ray/path tracer
            LightLockUnity.Cache(hash, realColor, 1f, permanent: false);
        }
    }

    Vector3 ExpensiveTrace() {
        // Replace with RayTracingAccelerationStructure, HDProbe, etc.
        return new Vector3(0.6f, 0.7f, 0.9f);
    }
}
