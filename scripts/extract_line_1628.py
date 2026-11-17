import json

path = r"C:\Users\miran\.gemini\antigravity\brain\b6afe388-0f80-47c0-9037-85deb561429f\.system_generated\logs\transcript_full.jsonl"
with open(path, "r", encoding="utf-8") as f:
    for idx, line in enumerate(f, 1):
        if idx == 1628:
            data = json.loads(line)
            # Find in tool_calls or content
            text = json.dumps(data)
            p = text.find("class MidasMCloudModal")
            if p != -1:
                print(text[p:p+3000].replace("\\n", "\n").replace('\\"', '"'))
            break

