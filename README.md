# zmk-behavior-CAPS-LOKS-ACCENTATE

Modulo ZMK che permette alle tastiere con layout **Italiano ISO (`ITA ISO`)** su **Windows** di scrivere automaticamente le vocali accentate maiuscole (**`È` `É` `À` `Ò` `Ù` `Ì`**) quando il **Caps Lock** è attivo.

---

## Compatibilità e Requisiti

> ⚠️ **IMPORTANTE — Leggere prima dell'uso**
> - **Sistema Operativo supportato:** **SOLO Windows**. Il modulo genera sequenze `Alt + 0xxx` sul tastierino numerico (Alt-code). **Non funziona** su macOS, Linux, Android o iOS/iPadOS.
> - **Layout di tastiera richiesto su Windows:** **Italiano (`ITA ISO`)** standard.
> - **Versione ZMK:** testato su **ZMK `v0.3`** (Zephyr 3.5+). Richiede `ZMK_HID_INDICATORS` (abilitato automaticamente dal `Kconfig` del modulo).
> - **Tastiere Split:** supportate (il codice viene compilato automaticamente solo su tastiere unibody o sul lato *Central* delle split).

---

## Comportamento dei tasti

Il modulo legge in tempo reale lo stato reale del **Caps Lock** dal PC tramite gli `HID Indicators` di ZMK (non un semplice toggle locale nella tastiera).

| Behavior | Tasto base (Caps OFF) | Caps OFF + Shift | Caps ON | Caps ON + Shift |
| :--- | :---: | :---: | :---: | :---: |
| **`&it_e`** | `è` (`LBKT`) | `é` | **`È`** (`Alt+0200`) | **`É`** (`Alt+0201`) |
| **`&it_a`** | `à` (`SQT`) | `°` | **`À`** (`Alt+0192`) | **`°`** |
| **`&it_o`** | `ò` (`SEMI`) | `ç` | **`Ò`** (`Alt+0210`) | **`ç`** |
| **`&it_u`** | `ù` (`BSLH`) | `§` | **`Ù`** (`Alt+0217`) | **`§`** |
| **`&it_i`** | `ì` (`EQUAL`) | `^` | **`Ì`** (`Alt+0204`) | **`^`** |

*Note tecniche:*
- Quando viene premuto `Shift + &it_e` con Caps Lock attivo, il driver maschera temporaneamente `Shift` per la durata della sequenza `Alt+0201` e rilascia `LALT` prima di smascherare `Shift`, evitando che Windows interpreti `Alt+Shift` come scorciatoia di cambio lingua.
- Tempi predefiniti della sequenza Alt-code: `tap-ms = <10>`, `wait-ms = <10>`.

---

## Come integrarlo in una tastiera ZMK (Guida passo-passo)

### 1. Aggiungi il modulo in `config/west.yml`
Nel repository `zmk-config` della tastiera, aggiungi il remote `GITTWA` e il progetto `zmk-behavior-CAPS-LOKS-ACCENTATE` dentro `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: GITTWA
      url-base: https://github.com/GITTWA
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: v0.3
      import: app/west.yml
    - name: zmk-behavior-CAPS-LOKS-ACCENTATE
      remote: GITTWA
      revision: main
  self:
    path: config
```

*(Nota: se la tastiera usa già altri moduli o una revisione diversa di ZMK, aggiungi solo il remote `GITTWA` e la voce del progetto sotto `projects:` mantenendo invariato il resto del file).*

### 2. Includi i behavior nella keymap (`.keymap`)
In cima al file `.keymap` della tastiera (sotto gli `#include` esistenti), aggiungi:

```dts
#include <behaviors/caps_acc.dtsi>
```

### 3. Sostituisci i 5 tasti nel layer base (`default_layer`)
Nella tua mappa tasti, sostituisci i codici standard della tastiera US corrispondenti alle accentate italiane con i nuovi behavior a 0 parametri:

- `&kp EQUAL` (tasto `ì / ^`) → **`&it_i`**
- `&kp LBKT` (tasto `è / é`) → **`&it_e`**
- `&kp SEMI` (tasto `ò / ç`) → **`&it_o`**
- `&kp SQT` (tasto `à / °`) → **`&it_a`**
- `&kp BSLH` (tasto `ù / §`) → **`&it_u`**

Esempio parziale di `default_layer`:
```dts
/* Fila numeri: ... 0 ' ì Backspace */
... &kp N0 &kp MINUS &it_i &kp BSPC

/* Fila QWERTY: ... O P è + */
... &kp O &kp P &it_e &kp RBKT

/* Fila ASDF: ... K L ò à ù */
... &kp K &kp L &it_o &it_a &it_u
```

### 4. Primo utilizzo su Bluetooth (IMPORTANTE)
Questo modulo attiva `CONFIG_ZMK_HID_INDICATORS=y` nel firmware per ricevere lo stato del Caps Lock dal PC. Questo modifica il descrittore HID della tastiera:
- **Via cavo USB:** funziona immediatamente dopo il flash.
- **Via Bluetooth:** la prima volta dopo aver installato il modulo, è necessario **rimuovere la tastiera** dai dispositivi Bluetooth di Windows, eseguire `&bt BT_CLR` sulla tastiera e **riassociarla da zero**.

---

## Personalizzazione avanzata (opzionale)

### Modificare la velocità di invio (`tap-ms` e `wait-ms`)
I valori predefiniti (`10 ms`) sono ottimizzati per funzionare in modo affidabile sia via USB sia via Bluetooth BLE. Se vuoi testarli a velocità diverse (es. `5 ms` su BLE veloce o `2 ms` solo su USB), puoi sovrascriverli nel `.keymap` dopo `#include <behaviors/caps_acc.dtsi>` senza modificare il modulo:

```dts
&it_e { tap-ms = <5>; wait-ms = <5>; };
&it_a { tap-ms = <5>; wait-ms = <5>; };
&it_o { tap-ms = <5>; wait-ms = <5>; };
&it_u { tap-ms = <5>; wait-ms = <5>; };
&it_i { tap-ms = <5>; wait-ms = <5>; };
```

> ⚠️ Valori troppo bassi su Bluetooth BLE possono causare perdita di cifre o errori nell'ordine dei caratteri (soprattutto su `0200`). In caso di errori, ripristinare `10 ms`.

### Creare un proprio behavior personalizzato con `zmk,behavior-caps-acc`
Il driver accetta le seguenti proprietà Devicetree (`#binding-cells = <0>`):

| Proprietà | Tipo | Obbligatoria | Descrizione |
| :--- | :--- | :---: | :--- |
| `bindings` | `phandle-array` | Sì | Binding base (es. `<&kp LBKT>`) usato a Caps OFF, o Caps ON + Shift se `alt-code-shifted` non è impostato. |
| `alt-code` | `int` | Sì | Codice Alt numerico Windows a 4 cifre senza lo zero iniziale (es. `200` invia `Alt+0200`). |
| `alt-code-shifted` | `int` | No | Codice Alt numerico opzionale quando **Caps Lock + Shift** sono entrambi attivi (es. `201` invia `Alt+0201`). |
| `tap-ms` | `int` | No (default `10`) | Durata in ms della pressione di ciascuna cifra sul tastierino numerico. |
| `wait-ms` | `int` | No (default `10`) | Pausa in ms tra un evento e il successivo della sequenza. |

---

## Struttura del repository (Riferimento tecnico)

```text
zmk-behavior-CAPS-LOKS-ACCENTATE/
├── CMakeLists.txt
├── Kconfig                                  # Attiva ZMK_BEHAVIOR_CAPS_ACC e seleziona ZMK_HID_INDICATORS
├── README.md
├── zephyr/
│   └── module.yml                           # Espone cmake, kconfig e dts_root: .
├── dts/
│   ├── behaviors/
│   │   └── caps_acc.dtsi                    # Definisce &it_e, &it_a, &it_o, &it_u, &it_i
│   └── bindings/
│       └── behaviors/
│           └── zmk,behavior-caps-acc.yaml   # Schema proprietà Devicetree
└── src/
    └── behaviors/
        └── behavior_caps_acc.c              # Driver C con macchina a stati asincrona (k_work_delayable)
```

---

## Nota rapida per integrazione su nuovi layout / AI assistant

Per integrare questo modulo in qualsiasi altra tastiera ZMK (`ITA ISO` su **Windows**):
1. Aggiungere `GITTWA/zmk-behavior-CAPS-LOKS-ACCENTATE` (`revision: main`) in `config/west.yml`.
2. Non è necessario aggiungere manualmente `CONFIG_ZMK_HID_INDICATORS=y` nel `.conf` (è già selezionato via `Kconfig` quando i nodi sono presenti nel Devicetree).
3. Aggiungere `#include <behaviors/caps_acc.dtsi>` nel `.keymap`.
4. Nel layer base sostituire: `EQUAL` → `&it_i`, `LBKT` → `&it_e`, `SEMI` → `&it_o`, `SQT` → `&it_a`, `BSLH` → `&it_u` (verificando prima il posizionamento fisico effettivo sul `.keymap` di destinazione, poiché alcune tastiere 60%/65%/split possono avere spostato questi tasti su layer o posizioni diverse).
5. Ricordare all'utente che il comportamento funziona **solo su Windows** con layout **Italiano ISO** e che al primo flash è necessario riassociare il Bluetooth.
