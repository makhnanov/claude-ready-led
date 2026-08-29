# claude-ready-led

مؤشر LED موصول بلوحة ESP8266، **ينطفئ عندما يبدأ Claude Code بالعمل** و**يضيء عندما
ينتهي**. بالإضافة إلى اختصار `Ctrl+T` يعمل على مستوى النظام لتبديله يدويًا.

[English](../README.md) ·
[Русский](README.ru.md) ·
[简体中文](README.zh-CN.md) ·
[Español](README.es.md) ·
[हिन्दी](README.hi.md) ·
**العربية** ·
[Português](README.pt-BR.md) ·
[Français](README.fr.md) ·
[Deutsch](README.de.md) ·
[日本語](README.ja.md) ·
[한국어](README.ko.md)

<p align="center">
  <img src="demo.gif" alt="المؤشر يتنفس أثناء عمل Claude Code" width="440">
  <img src="ui.jpg" alt="واجهة الويب التي تقدمها اللوحة بنفسها" width="277">
</p>

العتاد: لوحة NodeMCU v3 ‏(ESP8266) وأي مؤشر LED. البرمجيات: برنامج ثابت يتضمن خادم HTTP،
وخطافات (hooks) لـ Claude Code، واختصار عام في XFCE.

---

## تثبيت الخطاف

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh | bash
```

يستنسخ المستودع إلى `~/.claude-ready-led`، ويكتب الخطافات في `~/.claude/settings.json`
(مع الاحتفاظ بنسخة احتياطية في `settings.json.bak`)، ويحفظ عنوان الجهاز في
`~/.claude-led.conf`، ثم يتحقق من استجابة اللوحة. يمكن تشغيله مجددًا بأمان: فهو يستبدل
مدخلاته هو فقط ولا يمس أي خطافات أخرى.

إذا لم يُترجَم اسم mDNS، مرِّر العنوان صراحةً:

```bash
curl -fsSL https://raw.githubusercontent.com/makhnanov/claude-ready-led/main/scripts/install.sh \
  | CLAUDE_LED_URL=http://192.168.1.7 bash
```

بعد ذلك افتح `/hooks` داخل Claude Code (هذا يعيد تحميل الإعدادات) أو أعد تشغيله.

---

## التوصيل

المنفذ `D6` هو `GPIO12`.

```
D6 ──[ 220–330 Ω ]──▶| LED ──── GND
                    المصعد  المهبط
                  (الساق الطويلة) (الساق القصيرة،
                                   الحافة المسطحة)
```

المقاومة إلزامية: منفذ ESP8266 الواحد يعطي نحو 12 مللي أمبير كحد أقصى.

---

## البرنامج الثابت

```bash
cp firmware/claude_led/secrets.h.example firmware/claude_led/secrets.h
$EDITOR firmware/claude_led/secrets.h          # اسم الشبكة وكلمة المرور، 2.4 غيغاهرتز فقط

arduino-cli core install esp8266:esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --upload -p /dev/ttyUSB0 firmware/claude_led
```

الملف `secrets.h` مُدرج في `.gitignore`، لذا لا تصل كلمة مرور الشبكة إلى المستودع أبدًا.
المتتبَّع في git هو `secrets.h.example` فقط، وفيه قيم بديلة.

لمعرفة عنوان IP المخصص:

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200   # تطبع اللوحة عنوانها عند الإقلاع
ping claude-led.local                                    # أو عبر mDNS
```

---

## واجهة HTTP

| الطلب | الأثر |
|---|---|
| `GET /on` | إضاءة ثابتة |
| `GET /off` | إطفاء |
| `GET /toggle` | تبديل: مطفأ ← يضيء، وأي حالة أخرى ← ينطفئ |
| `GET /pulse` | تنفّس متدرج — Claude يعمل الآن |
| `GET /blink?times=3&ms=200` | وميض |
| `GET /done` | ثلاث ومضات سريعة ثم إضاءة ثابتة — انتهى العمل |
| `GET /status` | JSON: الوضع، العنوان، قوة الإشارة، مدة التشغيل |
| `GET /` | واجهة الويب (انظر أدناه) |

إذا ضبطت `API_TOKEN` في `secrets.h`، فكل طلب يحتاج إلى `?token=...`.

## سطر الأوامر

```bash
scripts/led.sh on | off | toggle | pulse | done | blink 5 100 | status
```

يُؤخذ العنوان من `CLAUDE_LED_URL`، ثم من `~/.claude-led.conf`، وأخيرًا
`http://claude-led.local`. يخرج السكربت دائمًا بالرمز 0 ولا يتوقف أكثر من ثانيتين، ولهذا
يصح تعليقه على خطاف: لوحة مفصولة أو غير متاحة لن تُبطئ Claude Code ولن تعطله.

---

## واجهة الويب

افتح `http://claude-led.local/` في المتصفح، من الحاسوب أو من الهاتف، فـ mDNS يعمل في
الشبكة المحلية كلها. تقدّم اللوحة صفحة فيها أزرار TOGGLE / ON / OFF / PULSE / BLINK /
DONE، و«مصباح» يحاكي الوضع الحالي، وحالة حية: العنوان وقوة الإشارة ومدة التشغيل.

**الأزرار لا تعيد تحميل الصفحة.** النقر يرسل `fetch("/on")`، ويعود الرد بصيغة JSON
فتُعاد رسمة الحالة والمصباح فورًا. كذلك تسأل الصفحة `/status` كل ثانيتين، فإذا بدّل
المؤشرَ خطافُ Claude Code أو الهاتف، لاحظت التبويبة ذلك. يتوقف الاستطلاع ما دامت
التبويبة مخفية (`document.hidden`) حتى لا تُرهَق اللوحة بلا داعٍ.

وإذا انقطع الاتصال، صار المصباح أحمر وظهرت عبارة «لا اتصال باللوحة» — فالصفحة لا تتجمد
صامتة أبدًا.

### أين يوجد كود الصفحة

لا يوجد ملف منفصل: اللوحة بلا نظام ملفات أصلًا. الصفحة كلها سلسلة نصية واحدة بلغة C++
داخل `firmware/claude_led/claude_led.ino`:

```cpp
static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html> ... )HTML";

void handleRoot() {
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}
```

يضعها `PROGMEM` في الذاكرة الوامضة بجوار الكود، ويرسلها `send_P` من هناك مباشرة دون
نسخها إلى الذاكرة العشوائية. ولهذا بالضبط تُطلب الحالة على حدة عبر `/status` بدل حقنها
في الـ HTML: تبقى الصفحة ساكنة ولا تحتاج ذاكرة عشوائية لتجميعها. وفّر هذا التحول نحو
650 بايت من أصل 80 كيلوبايت — وهو مقدار ملموس على متحكم دقيق.

إذا ضبطت `API_TOKEN` في `secrets.h`، فافتح الصفحة هكذا:
`http://claude-led.local/?token=xxxx` — ستلتقط الرمز من شريط العنوان وتضيفه إلى كل
طلباتها.

---

## خطافات Claude Code

ما يضيفه `install.sh` إلى `~/.claude/settings.json`:

```json
"hooks": {
  "UserPromptSubmit": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh off >/dev/null 2>&1", "timeout": 5 }] }],
  "Stop": [{ "hooks": [{ "type": "command",
    "command": "/path/to/scripts/led.sh done >/dev/null 2>&1", "timeout": 5 }] }]
}
```

- `UserPromptSubmit` — أرسلتَ طلبًا وبدأ Claude بالعمل: ينطفئ المؤشر.
- `Stop` — أنهى Claude إجابته: ثلاث ومضات ثم إضاءة ثابتة.

أي أن **الإضاءة تعني أن النتيجة جاهزة، والانطفاء يعني أنه ما زال يفكر**. وإن فضّلت أن
يتنفس أثناء العمل بدل أن ينطفئ، غيِّر `off` إلى `pulse` في الخطاف.

يُوجَّه الخرج إلى `/dev/null` عن قصد: وإلا فإن مخرجات خطاف `UserPromptSubmit` القياسية
تنتهي داخل سياق النموذج.

استخدم `/hooks` في Claude Code لمعاينتها أو تعطيلها.

---

## ‏Ctrl+T — تبديل المؤشر

```bash
scripts/hotkey-xfce.sh install    # للتفعيل
scripts/hotkey-xfce.sh status     # للفحص
scripts/hotkey-xfce.sh remove     # للإلغاء
```

يسجّل اختصارًا عامًا في XFCE عبر `xfconf`:

```
/commands/custom/<Primary>t  ->  /path/to/scripts/led.sh toggle
```

المطفأ يضيء، والمضيء أو المتنفس ينطفئ. يعمل على مستوى النظام، ويسري فورًا، ويبقى بعد
إعادة التشغيل.

**تنبيه:** يحجز XFCE هذا المفتاح حجزًا عامًا، فيتوقف `Ctrl+T` عن الوصول إلى التطبيقات —
ولن يفتح متصفحك تبويبة جديدة به بعد الآن. إن أزعجك ذلك، ألغِه بـ `remove` واربط تركيبة
أخرى:

```bash
KEY='<Primary><Alt>t' scripts/hotkey-xfce.sh install
```

ويمكن اختيار إجراء آخر بدل `toggle` بالطريقة نفسها:

```bash
KEY='<Super>l' ACTION=pulse scripts/hotkey-xfce.sh install
```

الإجراءات المتاحة: `toggle` (الافتراضي) و`on` و`off` و`pulse` و`done`.

### لا تستخدم XFCE؟

السكربت `hotkey-xfce.sh` يغطي XFCE فقط. وهذه المكافئات في البيئات الأخرى:

| بيئة سطح المكتب | طريقة الربط |
|---|---|
| GNOME | الإعدادات ← لوحة المفاتيح ← اختصارات مخصصة، والأمر `led.sh toggle` |
| KDE | إعدادات النظام ← الاختصارات ← اختصارات مخصصة |
| i3 / sway | `bindsym Control+t exec /path/to/led.sh toggle` في ملف الإعداد |
| Hyprland | `bind = CTRL, T, exec, /path/to/led.sh toggle` |
| X11 المجرد | `xbindkeys` مع `"led.sh toggle"` و`Control + t` في `~/.xbindkeysrc` |

---

## بنية المشروع

```
firmware/claude_led/claude_led.ino     البرنامج الثابت للوحة ESP8266
firmware/claude_led/secrets.h.example  قالب إعداد الشبكة (الملف secrets.h مستثنى في gitignore)
scripts/led.sh                         سطر الأوامر: on/off/toggle/pulse/blink/done/status
scripts/install.sh                     مثبّت خطافات Claude Code
scripts/hotkey-xfce.sh                 اختصار Ctrl+T العام
docs/                                  الصور وترجمات ملف README
```
