# GPS Tracker via SMS

**(Utilizando GPS NEO & SIM800L)**

## 📌 Visão Geral

Este projeto oferece uma solução eficiente para rastreamento GPS em tempo real e monitoramento remoto via SMS. O sistema integra um módulo GPS NEO para capturar dados de localização e um módulo GSM SIM800L para envio de alertas ou atualizações via mensagens de texto. É projetado para microcontroladores como o ESP32 e utiliza o framework ESP-IDF.

## 🚀 Funcionalidades

- 📍 **Rastreamento GPS em Tempo Real**: Obtém latitude, longitude, altitude e timestamp do módulo GPS.
- 📡 **Comunicação via SMS**: Envia mensagens contendo a localização atual.
- 🔔 **Alertas baseados em eventos**: Dispara mensagens quando determinados eventos ocorrem (ex.: saída de uma área delimitada).
- 🔋 **Otimização de consumo**: Projetado para operação com bateria, ideal para aplicações móveis/remotas.
- 🏗️ **Modular e escalável**: Pode ser expandido para incluir sensores adicionais e outras funcionalidades.

## 🛠️ Requisitos de Hardware

- **Microcontrolador**: ESP32 (ou equivalente, suportando ESP-IDF)
- **Módulo GPS**: GPS NEO (NEO-6M, NEO-7M, etc.)
- **Módulo GSM**: SIM800L
- **Fonte de Alimentação**: Regulada de 4V (mínimo 2A para picos de corrente do SIM800L)
- **Antenas**: Para GPS e GSM
- **Outros Componentes**: Conversores de nível lógico, fiação, resistores e capacitores conforme necessário

## 💻 Requisitos de Software

- **ESP-IDF**: Framework para desenvolvimento no ESP32
- **Biblioteca AT Commands**: Para comunicação com o SIM800L
- **Biblioteca de Parsing NMEA**: Para decodificação dos dados do GPS
- **FreeRTOS**: Para gerenciamento de tarefas no ESP-IDF

## ⚙️ Configuração e Instalação

### 1️⃣ Clonar o Repositório

```bash
git clone https://github.com/MatheusMnz/GpsTracker.git
cd GpsTracker
```

### 2️⃣ Configurar o Ambiente ESP-IDF

Certifique-se de que o ESP-IDF está instalado corretamente. Defina o alvo do chip ESP32 com:

```bash
idf.py set-target esp32
```

### 3️⃣ Configurar o Projeto

Abra o menu de configuração:

```bash
idf.py menuconfig
```

Ajuste os seguintes parâmetros:

- **UART e Baud Rate** para o GPS NEO
- **UART e Configuração do SIM800L** (incluindo APN se necessário)
- **Número de telefone para envio de SMS**
- **Intervalo de envio de mensagens ou eventos que disparam alertas**

### 4️⃣ Compilar e Enviar o Firmware

```bash
idf.py build
idf.py -p <PORTA> flash monitor
```

*(Substitua `<PORTA>` pelo identificador correto da porta serial.)*

## 🔄 Fluxo de Funcionamento

1. **Captura de Dados GPS**: O módulo GPS gera frases NMEA via UART. O firmware decodifica essas frases e extrai latitude, longitude e outros dados relevantes.
2. **Envio de SMS**: Após obter os dados de localização, o ESP32 se comunica com o SIM800L via comandos AT e envia mensagens de texto com a última localização registrada.
3. **Gatilhos de Eventos**: O sistema pode ser configurado para enviar mensagens ao detectar determinadas condições (ex.: geofencing, bateria baixa, etc.).

## 📖 Uso

- **Operação Normal**: O dispositivo inicia automaticamente o rastreamento GPS e o envio periódico de SMS ao ser ligado.
- **Pedido de Localização Manual**: O sistema pode responder a comandos SMS para enviar a localização sob demanda.
- **Depuração**: Utilize `idf.py monitor` para visualizar logs e depurar problemas.

## 🛠️ Solução de Problemas

- **Sem fixação de GPS**:
  - Verifique se o módulo tem uma visão clara do céu.
  - Confirme que a antena GPS está conectada corretamente.
  - Cheque as conexões UART entre ESP32 e GPS.

- **Problemas com SIM800L**:
  - Certifique-se de que a alimentação está estável.
  - Valide as conexões entre ESP32 e SIM800L.
  - Use comandos AT no monitor serial para depuração.

- **SMS não está sendo enviado**:
  - Confirme que o SIM está ativo e suporta SMS.
  - Verifique as configurações de APN (se aplicável).
  - Cheque o número de telefone configurado.

## 📌 Contribuindo

Contribuições são bem-vindas! Para colaborar:

1. Faça um fork do repositório.
2. Crie uma branch: `git checkout -b feature/nova-feature`.
3. Realize os commits com mensagens claras.
4. Abra um Pull Request para revisão.

## 📜 Licença

Este projeto é licenciado sob a Licença MIT. Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.

## 📞 Contato

Para dúvidas, sugestões ou problemas, abra uma issue no GitHub ou entre em contato pelo e-mail [seu.email@example.com](mailto:matheus.menezes0806@gmail.com).

---

✨ Desenvolvido por [MatheusMnz](https://github.com/MatheusMnz) ♦☺
