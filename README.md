# Prism – én-klik installer

Projektet bygger automatisk Arduino-sketch’en til:

- ESP32-S3-WROOM-1-N8R8
- 8 MB flash
- 8 MB OPI PSRAM
- USB CDC aktiveret
- WS2811-data på GPIO4
- Knap/input på GPIO38

Efter bygningen publiceres en HTTPS-side med knappen **Installér Prism**.

## Opret installer-linket

1. Opret en gratis GitHub-konto, hvis du ikke allerede har en.
2. Opret et nyt repository, eksempelvis `prism-installer`.
3. Udpak ZIP-filen og upload **indholdet** til repositoryets `main`-branch.
4. Åbn repositoryets **Settings → Pages**.
5. Under **Build and deployment → Source** vælges **GitHub Actions**.
6. Åbn fanen **Actions** og vent på, at workflowet
   `Build and publish Prism installer` er gennemført.
7. Det færdige link bliver normalt:
   `https://DIT-BRUGERNAVN.github.io/prism-installer/`

## Brug installer-linket

1. Åbn linket i Chrome eller Edge på en computer.
2. Tilslut ESP32’en med USB-datakabel.
3. Tryk **Installér Prism**.
4. Vælg ESP32’ens COM-port.
5. Efter genstart opretter enheden Wi-Fi-netværket `Prism Setup`.
6. Forbind telefonen til `Prism Setup`, og vælg dit normale Wi-Fi.
7. Åbn `http://prism.local`.

## Ændring af appnavn

I `Prism_RGB_Controller/Prism_RGB_Controller.ino` ændres:

```cpp
const char* APP_NAME  = "Prism";
const char* MDNS_NAME = "prism";
```

Push ændringen til GitHub. Workflowet bygger og publicerer automatisk
en ny firmwarefil.

## Sikkerhed

Wi-Fi-adgangskoden er ikke gemt i GitHub eller den offentlige firmwarefil.
Den indtastes lokalt gennem `Prism Setup` og gemmes direkte i ESP32’ens NVS.

## Browserkrav

USB-installationen bruger Web Serial. Brug Chrome eller Edge på en computer.
Safari og iPhone kan ikke udføre selve USB-flashningen.
