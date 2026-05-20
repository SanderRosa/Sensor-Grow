#include <DHT.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <EEPROM.h>

// ============================================================
//  PINOS
// ============================================================
#define DHTPIN    22
#define DHTTYPE   DHT22
#define SOLO1_PIN 32
#define SOLO2_PIN 34

// ============================================================
//  CONFIGURAÇÕES — EDITE AQUI
// ============================================================
const char* WIFI_SSID     = "Wokwi-GUEST";  // Troque para seu WiFi no hardware real
const char* WIFI_PASSWORD = "";              // Senha do WiFi
const char* DB_ENDPOINT   = "http://192.168.1.100:5000/api/grow/record"; // Seu servidor

// Ciclo de leituras
#define INTERVALO_MS      2000  // 2s entre leituras
#define LEITURAS_CICLO    30   // 30 leituras = ~1 minuto por ciclo
#define EEPROM_SLOTS      10   // 10 ciclos guardados na EEPROM

// ============================================================
//  EEPROM — Buffer Circular
//  [0]       = índice atual (uint8)
//  [1..161]  = 10 registros x 16 bytes (4 floats cada)
// ============================================================
#define EEPROM_SIZE  (1 + EEPROM_SLOTS * 16)
struct Registro { float temp, umid, solo1, solo2; };

// ============================================================
//  ESTADO GLOBAL
// ============================================================
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// Últimas leituras instantâneas (servidas ao dashboard)
float g_temp = 0, g_umid = 0, g_solo1 = 0, g_solo2 = 0;

// Acumuladores de ciclo
float ac_t = 0, ac_u = 0, ac_s1 = 0, ac_s2 = 0;
int   n_leituras = 0;
unsigned long t_ultima = 0;

// ============================================================
//  EEPROM — Funções
// ============================================================
void eepromSalvar(Registro r) {
  uint8_t idx = EEPROM.read(0) % EEPROM_SLOTS;
  int addr = 1 + idx * 16;
  EEPROM.put(addr, r);
  EEPROM.write(0, (idx + 1) % EEPROM_SLOTS);
  EEPROM.commit();
  Serial.printf("[EEPROM] Salvo no slot %d\n", idx);
}

// ============================================================
//  BANCO DE DADOS — HTTP POST
// ============================================================
void enviarDB(Registro r) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[DB] Sem WiFi - dado salvo apenas na EEPROM.");
    return;
  }
  HTTPClient http;
  http.begin(DB_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"temp\":" + String(r.temp,1)
              + ",\"umid\":" + String(r.umid,1)
              + ",\"solo1\":" + String((int)r.solo1)
              + ",\"solo2\":" + String((int)r.solo2) + "}";
  int code = http.POST(body);
  Serial.printf("[DB] POST %d\n", code);
  http.end();
}

// =========================================================================
// FRONT-END: HTML, CSS e JS (Colado direto aqui dentro)
// =========================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="3"> <!-- Atualiza a tela a cada 3 segundos -->
    <title>Sander's Holo-Tech Grow</title>
    <link href="https://fonts.googleapis.com/css2?family=Rajdhani:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-dark: #040814;
            --card-bg: #081224;
            --cyan: #00f3ff;
            --cyan-glow: rgba(0, 243, 255, 0.5);
            --green: #0de07d;
            --green-glow: rgba(13, 224, 125, 0.4);
            --yellow: #ffea00;
            --red: #ff3366;
            --text-main: #e0f7fa;
            --text-muted: #5e849c;
            --border-glow: rgba(0, 243, 255, 0.2);
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Rajdhani', sans-serif;
        }

        body {
            background-color: var(--bg-dark);
            color: var(--text-main);
            display: flex;
            justify-content: center;
            min-height: 100vh;
            padding: 10px;
            background-image: 
                linear-gradient(rgba(0, 243, 255, 0.03) 1px, transparent 1px),
                linear-gradient(90deg, rgba(0, 243, 255, 0.03) 1px, transparent 1px);
            background-size: 20px 20px;
        }

        .phone-mockup {
            width: 100%;
            max-width: 380px;
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding-bottom: 20px;
        }

        /* HEADER */
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px 5px;
        }

        .icon-btn {
            color: var(--cyan);
            background: rgba(0, 243, 255, 0.1);
            border: 1px solid var(--border-glow);
            border-radius: 8px;
            padding: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
            box-shadow: 0 0 10px rgba(0, 243, 255, 0.1);
        }

        .status-pill {
            font-size: 0.9rem;
            color: var(--green);
            font-weight: 600;
            letter-spacing: 1px;
            display: flex;
            align-items: center;
            gap: 8px;
            text-shadow: 0 0 8px var(--green-glow);
        }

        /* CARDS GERAIS */
        .holo-card {
            background: var(--card-bg);
            border: 1px solid var(--border-glow);
            border-radius: 12px;
            padding: 15px;
            position: relative;
            box-shadow: inset 0 0 20px rgba(0, 243, 255, 0.05), 0 5px 15px rgba(0,0,0,0.5);
            backdrop-filter: blur(5px);
        }

        /* Cantos Holográficos */
        .holo-card::before, .holo-card::after {
            content: ''; position: absolute; width: 10px; height: 10px;
        }
        .holo-card::before { top: -1px; left: -1px; border-top: 2px solid var(--cyan); border-left: 2px solid var(--cyan); border-radius: 12px 0 0 0; }
        .holo-card::after { bottom: -1px; right: -1px; border-bottom: 2px solid var(--cyan); border-right: 2px solid var(--cyan); border-radius: 0 0 12px 0; }

        /* SEÇÃO 1: ISO CUBE & MINIS */
        .top-grid {
            display: grid;
            grid-template-columns: 1.2fr 1fr;
            gap: 12px;
        }

        .iso-box {
            display: flex;
            justify-content: center;
            align-items: center;
            height: 160px;
            position: relative;
        }

        .iso-svg {
            width: 100%;
            height: 100%;
            filter: drop-shadow(0 0 5px var(--cyan-glow));
            animation: float 4s ease-in-out infinite;
        }

        @keyframes float {
            0%, 100% { transform: translateY(0); }
            50% { transform: translateY(-5px); }
        }

        .mini-stats {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .mini-card {
            background: rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(0, 243, 255, 0.15);
            border-radius: 8px;
            padding: 10px;
            flex: 1;
            display: flex;
            flex-direction: column;
            justify-content: center;
            position: relative;
            overflow: hidden;
        }

        .mini-card-title { font-size: 0.7rem; color: var(--text-muted); text-transform: uppercase; }
        .mini-card-val { font-size: 1.5rem; font-weight: 700; color: var(--cyan); text-shadow: 0 0 8px var(--cyan-glow); }
        
        .mini-card-bg {
            position: absolute; right: -10px; bottom: -10px; opacity: 0.2; width: 50px;
        }

        /* SEÇÃO 2: MIDDLE CHART */
        .mid-section {
            display: grid;
            grid-template-columns: 80px 1fr;
            gap: 15px;
            align-items: center;
        }

        .dome-svg {
            width: 100%;
            filter: drop-shadow(0 0 5px var(--green-glow));
        }

        .chart-container {
            width: 100%;
            height: 80px;
            position: relative;
        }

        .chart-svg { width: 100%; height: 100%; overflow: visible; }
        .chart-line { fill: none; stroke: var(--cyan); stroke-width: 2; filter: drop-shadow(0 0 4px var(--cyan-glow)); }
        .chart-fill { fill: url(#cyanGrad); opacity: 0.3; }

        .chart-labels {
            display: flex; justify-content: space-between;
            font-size: 0.6rem; color: var(--text-muted); margin-top: 5px;
        }

        /* SEÇÃO 3: RINGS & LEGEND */
        .bot-grid {
            display: grid;
            grid-template-columns: 120px 1fr;
            gap: 20px;
            align-items: center;
        }

        .rings-container {
            position: relative;
            width: 120px;
            height: 120px;
        }

        .ring-svg { width: 100%; height: 100%; transform: rotate(-90deg); }
        .ring-bg { fill: none; stroke: rgba(255,255,255,0.05); }
        .ring-prog { fill: none; stroke-linecap: round; transition: stroke-dashoffset 1s ease; }

        .ring-center-icon {
            position: absolute;
            top: 50%; left: 50%; transform: translate(-50%, -50%);
            color: var(--cyan);
            filter: drop-shadow(0 0 5px var(--cyan-glow));
        }

        .legend-list {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .legend-item {
            display: flex;
            align-items: center;
            justify-content: space-between;
            font-size: 0.8rem;
            font-weight: 600;
        }

        .legend-label { display: flex; align-items: center; gap: 8px; color: var(--text-main); }
        .legend-dot { width: 8px; height: 8px; border-radius: 50%; }

        /* GALERIA DE HARDWARE */
        .hw-title {
            font-size: 0.8rem;
            color: var(--cyan);
            margin-top: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
            display: flex;
            align-items: center;
            gap: 5px;
        }
        .hw-title::before { content:''; width: 5px; height: 5px; background: var(--cyan); }

        .hw-grid {
            display: flex;
            gap: 10px;
            overflow-x: auto;
            padding-bottom: 5px;
            scrollbar-width: thin;
            scrollbar-color: var(--cyan) transparent;
        }

        .hw-item {
            min-width: 70px;
            height: 70px;
            border: 1px solid var(--border-glow);
            border-radius: 8px;
            position: relative;
            background: #000;
            flex-shrink: 0;
            overflow: hidden;
        }

        .hw-item img {
            width: 100%; height: 100%; object-fit: cover;
            opacity: 0.6; filter: sepia(1) hue-rotate(150deg) saturate(2);
            transition: 0.3s;
        }
        .hw-item:hover img { opacity: 1; filter: none; }

        .hw-tag {
            position: absolute; bottom: 0; width: 100%;
            background: rgba(0,243,255,0.7); color: #000;
            font-size: 0.5rem; text-align: center; font-weight: 700;
        }

        /* BOTTOM NAV */
        .bottom-nav {
            display: flex;
            justify-content: space-around;
            align-items: center;
            background: var(--card-bg);
            border: 1px solid var(--border-glow);
            border-radius: 20px;
            padding: 12px 20px;
            margin-top: auto;
            box-shadow: 0 10px 20px rgba(0,0,0,0.5);
        }

        .nav-icon {
            width: 24px; height: 24px; color: var(--text-muted);
            transition: 0.3s; cursor: pointer;
        }
        .nav-icon.active {
            color: var(--green); filter: drop-shadow(0 0 5px var(--green-glow));
        }

    </style>
</head>
<body>

<div class="phone-mockup">
    <!-- HEADER -->
    <header>
        <div class="icon-btn">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="3" y1="12" x2="21" y2="12"></line><line x1="3" y1="6" x2="21" y2="6"></line><line x1="3" y1="18" x2="21" y2="18"></line></svg>
        </div>
        <div class="status-pill">Status: Optimizing</div>
        <div class="icon-btn">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"></circle><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path></svg>
        </div>
    </header>

    <!-- TOP GRID (Cubo e Valores dos Vasos) -->
    <div class="top-grid">
        <div class="holo-card iso-box">
            <svg class="iso-svg" viewBox="0 0 100 100">
                <!-- Cubo Isométrico Wireframe -->
                <path d="M50 15 L85 35 L50 55 L15 35 Z" fill="rgba(0, 243, 255, 0.05)" stroke="var(--cyan)" stroke-width="1"/>
                <path d="M15 35 L50 55 L50 95 L15 75 Z" fill="rgba(0, 243, 255, 0.02)" stroke="var(--cyan)" stroke-width="1"/>
                <path d="M85 35 L50 55 L50 95 L85 75 Z" fill="rgba(0, 243, 255, 0.08)" stroke="var(--cyan)" stroke-width="1"/>
                <!-- Planta Interna -->
                <path d="M50 85 Q45 60 50 30" fill="none" stroke="var(--green)" stroke-width="2" stroke-linecap="round"/>
                <path d="M50 65 Q35 60 30 45" fill="none" stroke="var(--green)" stroke-width="1.5" stroke-linecap="round"/>
                <path d="M50 55 Q65 50 70 35" fill="none" stroke="var(--green)" stroke-width="1.5" stroke-linecap="round"/>
                <!-- Glow points -->
                <circle cx="50" cy="30" r="2" fill="#fff"/>
                <circle cx="30" cy="45" r="1.5" fill="#fff"/>
                <circle cx="70" cy="35" r="1.5" fill="#fff"/>
                <circle cx="50" cy="55" r="1.5" fill="#fff"/>
            </svg>
        </div>
        <div class="mini-stats">
            <div class="mini-card">
                <div class="mini-card-title">Pot 1 Moisture</div>
                <div class="mini-card-val"><span id="soil1-text">{{SOLO1}}</span>%</div>
                <svg class="mini-card-bg" viewBox="0 0 24 24" fill="none" stroke="var(--cyan)"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
            </div>
            <div class="mini-card">
                <div class="mini-card-title">Pot 2 Moisture</div>
                <div class="mini-card-val" style="color:var(--green); text-shadow:0 0 8px var(--green-glow);"><span id="soil2-text">{{SOLO2}}</span>%</div>
                <svg class="mini-card-bg" viewBox="0 0 24 24" fill="none" stroke="var(--green)"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
            </div>
        </div>
    </div>

    <!-- MIDDLE SECTION (Gráfico e Dome) -->
    <div class="holo-card mid-section">
        <div class="dome-container">
            <svg class="dome-svg" viewBox="0 0 100 100">
                <!-- Dome Base -->
                <ellipse cx="50" cy="80" rx="35" ry="10" fill="none" stroke="var(--cyan)" stroke-width="2"/>
                <!-- Dome Glass -->
                <path d="M15 80 Q15 20 50 20 Q85 20 85 80" fill="rgba(0, 243, 255, 0.05)" stroke="var(--cyan)" stroke-width="1.5" stroke-dasharray="4 4"/>
                <!-- Mini Plant -->
                <path d="M50 80 Q40 60 50 40 Q60 50 70 45" fill="none" stroke="var(--green)" stroke-width="2"/>
                <path d="M50 65 Q40 55 30 50" fill="none" stroke="var(--green)" stroke-width="2"/>
            </svg>
        </div>
        <div>
            <div style="font-size: 0.7rem; color: var(--cyan); margin-bottom: 5px;">ENVIRONMENTAL DATA</div>
            <div class="chart-container">
                <svg class="chart-svg" viewBox="0 0 100 40" preserveAspectRatio="none">
                    <defs>
                        <linearGradient id="cyanGrad" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="0%" stop-color="var(--cyan)" />
                            <stop offset="100%" stop-color="transparent" />
                        </linearGradient>
                    </defs>
                    <path class="chart-fill" d="M0,40 L0,20 Q15,5 30,25 T60,15 T85,25 L100,10 L100,40 Z" />
                    <path class="chart-line" d="M0,20 Q15,5 30,25 T60,15 T85,25 L100,10" />
                </svg>
            </div>
            <div class="chart-labels"><span>00:00</span><span>12:00</span><span>24:00</span></div>
        </div>
    </div>

    <!-- BOTTOM SECTION (Rings & Legends) -->
    <div class="holo-card bot-grid">
        <div class="rings-container">
            <svg class="ring-svg" viewBox="0 0 100 100">
                <!-- Ring 1: Temp (Cyan) r=40, c=251.2 -->
                <circle class="ring-bg" cx="50" cy="50" r="40" stroke-width="4"/>
                <circle id="ring-temp" class="ring-prog" cx="50" cy="50" r="40" stroke-width="4" stroke="var(--cyan)" stroke-dasharray="251.2" stroke-dashoffset="251.2" style="filter: drop-shadow(0 0 3px var(--cyan-glow))"/>
                
                <!-- Ring 2: Humid (Green) r=30, c=188.5 -->
                <circle class="ring-bg" cx="50" cy="50" r="30" stroke-width="4"/>
                <circle id="ring-umid" class="ring-prog" cx="50" cy="50" r="30" stroke-width="4" stroke="var(--green)" stroke-dasharray="188.5" stroke-dashoffset="188.5" style="filter: drop-shadow(0 0 3px var(--green-glow))"/>

                <!-- Ring 3: Soil 1 (Yellow) r=20, c=125.6 -->
                <circle class="ring-bg" cx="50" cy="50" r="20" stroke-width="4"/>
                <circle id="ring-soil1" class="ring-prog" cx="50" cy="50" r="20" stroke-width="4" stroke="var(--yellow)" stroke-dasharray="125.6" stroke-dashoffset="125.6"/>

                <!-- Ring 4: Soil 2 (Red) r=10, c=62.8 -->
                <circle class="ring-bg" cx="50" cy="50" r="10" stroke-width="4"/>
                <circle id="ring-soil2" class="ring-prog" cx="50" cy="50" r="10" stroke-width="4" stroke="var(--red)" stroke-dasharray="62.8" stroke-dashoffset="62.8"/>
            </svg>
            <div class="ring-center-icon">
                <svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor"><polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"></polygon></svg>
            </div>
        </div>
        
        <div class="legend-list">
            <div class="legend-item">
                <div class="legend-label"><div class="legend-dot" style="background: var(--cyan);"></div> Temperature</div>
                <div style="color: var(--cyan);" id="val-temp">{{TEMP}}°</div>
            </div>
            <div class="legend-item">
                <div class="legend-label"><div class="legend-dot" style="background: var(--green);"></div> Humidity</div>
                <div style="color: var(--green);" id="val-umid">{{UMID}}%</div>
            </div>
            <div class="legend-item">
                <div class="legend-label"><div class="legend-dot" style="background: var(--yellow);"></div> Soil Pot 1</div>
                <div style="color: var(--yellow);" id="val-s1">{{SOLO1}}%</div>
            </div>
            <div class="legend-item">
                <div class="legend-label"><div class="legend-dot" style="background: var(--red);"></div> Soil Pot 2</div>
                <div style="color: var(--red);" id="val-s2">{{SOLO2}}%</div>
            </div>
        </div>
    </div>

    <!-- HARDWARE LOG (Imagens originais do usuário) -->
    <div style="display:none;"> <!-- Ocultado por padrão se não quiser imagens no app real -->
        <div class="hw-title">Hardware Log</div>
        <div class="hw-grid">
            <div class="hw-item">
                <img src="image_0962f8.png" alt="ESP32">
            </div>
        </div>
    </div>

</div>

<script>
    function setValues(t, u, s1, s2) {
        t = parseFloat(t); u = parseFloat(u);
        s1 = parseInt(s1); s2 = parseInt(s2);

        document.getElementById('val-temp').innerText   = t.toFixed(1) + '\u00B0C';
        document.getElementById('val-umid').innerText   = u.toFixed(1) + '%';
        document.getElementById('soil1-text').innerText = s1 + '%';
        document.getElementById('soil2-text').innerText = s2 + '%';
        document.getElementById('val-s1').innerText     = s1 + '%';
        document.getElementById('val-s2').innerText     = s2 + '%';

        document.getElementById('ring-temp').style.strokeDashoffset  = 251.2 - (251.2 * Math.min(t / 50, 1));
        document.getElementById('ring-umid').style.strokeDashoffset  = 188.5 - (188.5 * Math.min(u / 100, 1));
        document.getElementById('ring-soil1').style.strokeDashoffset = 125.6 - (125.6 * Math.min(s1 / 100, 1));
        document.getElementById('ring-soil2').style.strokeDashoffset = 62.8  - (62.8  * Math.min(s2 / 100, 1));
    }

    // Inicializa com os valores ja inseridos pelo servidor C++
    setValues('{{TEMP}}', '{{UMID}}', '{{SOLO1}}', '{{SOLO2}}');

    // Atualiza via fetch a cada 3s (sem recarregar a pagina)
    setInterval(function() {
        fetch('/data')
            .then(function(r) { return r.json(); })
            .then(function(d) { setValues(d.temp, d.umid, d.solo1, d.solo2); })
            .catch(function() { /* silencioso se perder conexao */ });
    }, 3000);
</script>

</body>
</html>
)rawliteral";
// =========================================================================

// ============================================================
//  SERVIDOR WEB — Página Principal
// ============================================================
void handleRoot() {
  // Usa os globais (já atualizados pelo loop)
  String s = INDEX_HTML;
  s.replace("{{TEMP}}",  String(g_temp, 1));   // 1 decimal: 24.5
  s.replace("{{UMID}}",  String(g_umid, 1));   // 1 decimal: 65.3
  s.replace("{{SOLO1}}", String((int)g_solo1)); // inteiro: 72
  s.replace("{{SOLO2}}", String((int)g_solo2));
  server.send(200, "text/html", s);
}

// Endpoint JSON para consumo externo / apps
void handleData() {
  String j = "{";
  j += "\"temp\":"  + String(g_temp,1)  + ",";
  j += "\"umid\":"  + String(g_umid,1)  + ",";
  j += "\"solo1\":" + String((int)g_solo1) + ",";
  j += "\"solo2\":" + String((int)g_solo2);
  j += "}";
  server.send(200, "application/json", j);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  dht.begin();
  delay(500);

  Serial.println("\n=== Sander's Grow Lab ===");

  // WiFi
  Serial.print("Conectando WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 20) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConectado! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nFalha WiFi - modo offline.");
  }

  if (MDNS.begin("grow")) Serial.println("mDNS: http://grow.local");

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Servidor Web na porta 80.");
}

// ============================================================
//  LOOP — Leitura + Ciclos + EEPROM + DB
// ============================================================
void loop() {
  server.handleClient();

  unsigned long agora = millis();
  if (agora - t_ultima < INTERVALO_MS) return;
  t_ultima = agora;

  // Leitura DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("[SENSOR] Falha DHT22 - pulando.");
    return;
  }

  // Leitura Solo
  int s1 = constrain(map(analogRead(SOLO1_PIN), 0, 4095, 0, 100), 0, 100);
  int s2 = constrain(map(analogRead(SOLO2_PIN), 0, 4095, 0, 100), 0, 100);

  // Atualiza globais (para o dashboard em tempo real)
  g_temp = t; g_umid = h;
  g_solo1 = s1; g_solo2 = s2;

  Serial.printf("Temp: %.1fC | Umid: %.1f%% | Solo1: %d%% | Solo2: %d%%\n",
                t, h, s1, s2);

  // Acumula para o ciclo
  ac_t  += t;  ac_u  += h;
  ac_s1 += s1; ac_s2 += s2;
  n_leituras++;

  // Fim de ciclo → média → EEPROM → DB
  if (n_leituras >= LEITURAS_CICLO) {
    Registro media = {
      ac_t  / n_leituras,
      ac_u  / n_leituras,
      ac_s1 / n_leituras,
      ac_s2 / n_leituras
    };
    Serial.printf("[CICLO] Media: %.1fC / %.1f%% / %d%% / %d%%\n",
                  media.temp, media.umid, (int)media.solo1, (int)media.solo2);
    eepromSalvar(media);
    enviarDB(media);

    // Reconecta WiFi se necessário
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Reconectando...");
      WiFi.reconnect();
      delay(3000);
    }

    // Zera acumuladores
    ac_t = ac_u = ac_s1 = ac_s2 = 0;
    n_leituras = 0;
  }
}