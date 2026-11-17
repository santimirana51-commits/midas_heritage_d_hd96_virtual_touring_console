log_path = r"C:\Users\miran\.gemini\antigravity\brain\b6afe388-0f80-47c0-9037-85deb561429f\.system_generated\tasks\task-3558.log"
with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
    text = f.read()

idx = text.find("class MidasMCloudModal")
if idx != -1:
    print(text[idx:idx+2500])
else:
    print("Not found")

