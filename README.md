# 🌱 Sensor Grow IoT — Monitoramento com Dashboard Holográfico

Nó sensor inteligente (IoT) projetado para controle do microclima em cultivos (Grow Rooms). Baseado na arquitetura ESP32, ele realiza leitura ambiental, armazena logs localmente na memória física para tolerância a falhas e hospeda um servidor Web com uma interface estilo holográfica futurista.

## 🚀 Funcionalidades
- **Sensoriamento Ambiental**: Amostragem simultânea de temperatura e umidade do ar (via DHT22) e umidade de múltiplos solos (analógicos capacitivos).
- **Dashboard Web Cyberpunk / Holográfico**: O ESP32 hospeda internamente um dashboard (HTML5/CSS3/JS avançado) com gauges virtuais, sem precisar de internet ou servidor externo.
- **Resiliência de Dados (EEPROM)**: Se a comunicação Wi-Fi cair, o nó acumula os dados no buffer circular fixo diretamente na EEPROM (memória flash local) e sincroniza os blocos após a conexão voltar.
- **Sincronização Cloud HTTP POST**: Envio de pacotes `JSON` consolidados para endpoints e banco de dados via rede wireless.

## 🛠️ Tecnologias Utilizadas
- **C++ (Arduino Core)** (Firmware da placa e servidor assíncrono Web)
- **ESP32** (Microcontrolador com recursos de conectividade)
- **Wokwi** (Plataforma e ecossistema de simulação ciber-física)
- **Web Frontend** (CSS3 Avançado para efeitos neon/glassmorphism em UI IoT)
- **Componentes Eletrônicos**: DHT22, Sensores Higrômetros.

## ⚙️ Como Executar (Emulação Completa)
O projeto suporta ser executado em hardware real ou totalmente simulado usando o emulador virtual **Wokwi**, graças ao diagrama e configurações integrados.

1. Baixe e instale a extensão do **Wokwi Simulator** no seu VS Code.
2. Abra a pasta deste projeto (`Sensor_Grow`) na IDE.
3. Inicie o simulador através do arquivo `wokwi.toml` (botão de iniciar simulação do Wokwi).
4. O terminal de emulação criará um IP local (virtual). Segure o `Ctrl` e clique no IP para ver o dashboard de monitoramento operando simultaneamente com o circuito animado.

## 📸 Demonstração
*(Espaço reservado para prints da interface UI Holográfica e do circuito)*
