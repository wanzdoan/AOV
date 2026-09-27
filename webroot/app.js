const bridge = {
  available() {
    return typeof window.ksu !== "undefined" && typeof window.ksu.exec === "function";
  },
  exec(command, options = {}) {
    if (!this.available()) {
      return Promise.resolve({ errno: -1, stdout: "", stderr: "KernelSU bridge not available" });
    }
    return new Promise(resolve => {
      window.ksu.exec(command, JSON.stringify(options), (errno, stdout, stderr) => {
        resolve({ errno, stdout: stdout || "", stderr: stderr || "" });
      });
    });
  },
  toast(message) {
    if (this.available() && typeof window.ksu.toast === "function") window.ksu.toast(message);
  }
};

const consoleBox = document.getElementById("console");
const status = document.getElementById("status");
const gameState = document.getElementById("gameState");
const injectState = document.getElementById("injectState");

function setConsole(text) {
  const stamp = new Date().toLocaleTimeString();
  consoleBox.textContent = `[${stamp}] ${text || "Khong co du lieu."}`;
}

function setStatus(kind, label) {
  status.className = `status ${kind}`;
  status.querySelector("b").textContent = label;
}

async function inspectRuntime(showOutput = true) {
  if (!bridge.available()) {
    setStatus("off", "Browser mode");
    gameState.textContent = "Khong ro";
    injectState.textContent = "Khong co bridge";
    setConsole("Mo WebUI tu KernelSU Next Manager de chay chan doan.");
    return;
  }

  setStatus("waiting", "Dang kiem tra");
  const command = [
    'PID="$(pidof com.garena.game.kgvn 2>/dev/null)"',
    'echo "game_pid=${PID:-none}"',
    'echo "module=$(test -s /data/adb/modules/aov_zygisk/zygisk/arm64-v8a.so && echo ready || echo missing)"',
    'logcat -d -s aov_zygisk 2>/dev/null | tail -n 24'
  ].join('; ');
  const result = await bridge.exec(command);
  const output = `${result.stdout}${result.stderr ? `\n${result.stderr}` : ""}`.trim();
  const running = /game_pid=(?!none)\S+/.test(output);
  const injected = /AOV Zygisk loaded|ImGui ready|data ready/.test(output);

  gameState.textContent = running ? "Dang chay" : "Chua chay";
  injectState.textContent = injected ? "Da nap" : "Cho";
  setStatus(injected ? "ready" : (running ? "waiting" : "off"),
            injected ? "Da ket noi" : (running ? "Dang nap" : "Cho game"));
  if (showOutput) setConsole(output || "Khong tim thay log aov_zygisk.");
}

document.getElementById("refresh").addEventListener("click", () => inspectRuntime(false));
document.getElementById("check").addEventListener("click", () => inspectRuntime(true));
document.getElementById("logs").addEventListener("click", async () => {
  const result = await bridge.exec("logcat -d -s aov_zygisk 2>/dev/null | tail -n 80");
  setConsole((result.stdout || result.stderr).trim() || "Khong co log aov_zygisk.");
});
document.getElementById("stop").addEventListener("click", async () => {
  if (!confirm("Dung tien trinh AOV hien tai?")) return;
  const result = await bridge.exec("am force-stop com.garena.game.kgvn");
  if (result.errno === 0) {
    bridge.toast("Da dung AOV");
    setConsole("Da dung AOV. Mo game lai theo cach thong thuong.");
    await inspectRuntime(false);
  } else {
    setConsole(result.stderr || "Khong the dung AOV.");
  }
});

window.addEventListener("DOMContentLoaded", () => inspectRuntime(true));
