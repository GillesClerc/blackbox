# PCM5122 Register Reference

Source: TI PCM5122 datasheet (SLAS852) + esphome-pcm5122 (sonocotta/esphome-pcm5122)

## Page 0 (default)

| Register | Name      | Description                                |
|----------|-----------|--------------------------------------------|
| 0x00     | PAGE_SET  | Page selection (0x00=page0, 0x01=page1)    |
| 0x02     | STATE     | Control state (0x00=run, 0x10=standby)     |
| 0x03     | MUTE      | Mute control (0x00=unmute, 0x11=mute both) |
| 0x04     | PLL_EN    | PLL enable (0x01=enable)                   |
| 0x0D     | PLL_SRC   | PLL source (0x10=BCK)                      |
| 0x0E     | DAC_CLK   | DAC clock source (0x10=PLL)                |
| 0x14     | PLL_P     | PLL P divider                              |
| 0x15     | PLL_J     | PLL J multiplier                           |
| 0x16     | PLL_D_HI  | PLL D fractional [13:8]                    |
| 0x17     | PLL_D_LO  | PLL D fractional [7:0]                     |
| 0x18     | PLL_R     | PLL R divider                              |
| 0x1B     | DSP_DIV   | DSP clock divider                          |
| 0x1C     | DAC_DIV   | DAC clock divider                          |
| 0x1D     | NCP_DIV   | NCP clock divider                          |
| 0x1E     | OSR       | Oversampling ratio (0x00=auto)             |
| 0x25     | IGN_ERR   | Ignore clock errors                        |
| 0x28     | I2S_FMT   | I2S data format (0x00=I2S standard)        |
| 0x2A     | MIXER     | Mixer mode (0x11=stereo)                   |
| 0x2B     | DSP_PROG  | DSP filter program (1-7)                   |
| 0x3D     | VOL_L     | Digital volume left channel                |
| 0x3E     | VOL_R     | Digital volume right channel               |
| 0x3F     | VOL_RAMP  | Volume ramp rate                           |

## Digital Volume (0x3D / 0x3E)

Range: -103 dB to +24 dB, pas de 0.5 dB.
Formule: raw = (dB - 24) * -2

| Valeur (hex) | Valeur (dec) | dB     |
|--------------|--------------|--------|
| 0x00         | 0            | +24 dB |
| 0x30         | 48           | 0 dB   |
| 0x3C         | 60           | -6 dB  |
| 0xFE         | 254          | -103 dB|
| 0xFF         | 255          | mute   |

Valeur plus haute = plus attenue (volume plus bas).
0x30 = 0 dB est un bon defaut.

## Page 1

## Analog Gain (Page 1, register 0x02)

IMPORTANT: Ce registre est sur la PAGE 1 !
Sequence: pcm_write(0x00, 0x01) → pcm_write(0x02, val) → pcm_write(0x00, 0x00)

| Valeur | Gain              |
|--------|--------------------|
| 0x00   | 0 dB (2 Vrms)     |
| 0x11   | -6 dB (1 Vrms)    |

Seulement 2 niveaux.

## DSP Filter Programs (Page 0, 0x2B)

IMPORTANT: Changer le filtre REQUIERT standby !
Sequence: pcm_write(0x02, 0x10) → pcm_write(0x2B, prog) → pcm_write(0x02, 0x00)

| Valeur | Programme                        |
|--------|----------------------------------|
| 0x01   | Low latency IIR                  |
| 0x03   | High attenuation                 |
| 0x05   | Apodizing FIR                    |
| 0x06   | Brick-wall                       |
| 0x07   | Ringing-less low latency FIR     |

Note: la difference entre filtres est subtile — affecte les transitoires
et le roll-off pres de Nyquist, pas un EQ audible.

## Mute (Page 0, 0x03)

| Valeur | Etat                |
|--------|---------------------|
| 0x00   | Unmute (both ch)    |
| 0x11   | Mute (both ch)      |

## Volume Ramp (Page 0, 0x3F)

Bits [6:4] = ramp-up speed, bits [2:0] = ramp-down speed.
Valeur plus haute = plus rapide.
