# VonBerg Harmony

Vokal harmoni eklentisi (VST3, Windows, FL Studio).
Vokali bırak → ton ve notalar otomatik bulunur → harmoniyi seç ve nota nota düzenle → sonucu **playlist'e sürükle**.

## Nasıl kullanılır
1. Plugin'i herhangi bir mixer kanalına ekle (ses geçirir, ön dinleme o kanaldan çıkar).
2. Vokali büyük ekrana **sürükle bırak**: FL Browser (*Current project → Audio clips* içinde projedeki tüm vokal klipleri var), Windows Gezgini;
   FL sürümün izin veriyorsa doğrudan playlist'teki klipten de.
   Tıklayıp dosya da seçebilirsin, ya da **REC** ile kanaldaki vokali kaydedersin (FL çalıyorsa bir sonraki bardan başlar, en fazla 60 sn).
3. Notalar ve ton bulunur, **KEY / SCALE** otomatik ayarlanır (**AUTO** = tekrar algılanan tona dön). Makam için *Hicaz* ve *Phrygian / Kürdi* gamları da var.
4. **PRESET** ◀ ▶ ile 24 hazır harmoniden birini seç ya da **RANDOM**.
5. **PREVIEW** ile dinle (üstteki zaman şeridine tıklarsan oradan çalar).
6. Sağ alttaki kutulardan birini **basılı tutup playlist'e sürükle**:
   - **MIX**: vokal + harmoniler
   - **HARMONIES**: sadece harmoniler (orijinal vokalin altına koy)
   - **STEMS**: her ses ayrı WAV (FL her birini ayrı kanala koyar)

   Dosyalar ayrıca `Belgeler\VonBerg Harmony\` içine kaydedilir (kutuya tıklayınca klasör açılır).
   Klibi orijinal vokalin başladığı yere bırak; uzunluk ve başlangıç aynıdır.

## 4 harmoni sesi (A B C D)
| Ayar | Ne yapar |
|---|---|
| ON | Sesi aç / kapat |
| SCALE / SEMI | **SCALE**: gama göre (3rd Up = tona uygun üçlü, majör/minör kendiliğinden doğru çıkar). **SEMI**: sabit yarım ses (+12 = oktav) |
| ◀ Aralık ▶ | Aralığı değiştir; ortadaki yazıya tıklarsan tüm liste açılır |
| LEVEL / PAN | Ses seviyesi ve stereo konum |
| FORMANT | Ses rengi: eksi = kalın/erkeksi, artı = ince/kadınsı (perdeyi değiştirmez) |
| DELAY | Gecikme (ms): doğal, ayrı kişi söylüyormuş hissi; double için 15–35 ms |
| DETUNE | İnce perde kaydırma (cent): double / chorus etkisi |
| HUMAN | Yavaş, doğal perde kayması |

**Genel:** LEAD VOCAL (mix'teki ana vokal), HARMONIES (harmoni seviyesi), BREATH (harmonilerdeki ünsüz / nefes miktarı),
PITCH LOCK (harmonileri tam notaya kilitler; %100 = robotik), WIDTH (stereo genişlik).

## Nota düzenleme
- Üstteki **EDIT VOICE A B C D** ile düzenlenecek sesi seç (ya da sesin kutusuna tıkla).
- Renkli notayı **yukarı / aşağı sürükle**: gamdaki notalara oturur, **Alt** basılıyken yarım ses yarım ses gider.
- **Çift tık** = o notayı sustur (kesik çizgi olarak kalır, tekrar çift tık = aç).
- **Sağ tık** menüsü: sustur, sıfırla, ±1 yarım ses, ±1 oktav, tüm düzenlemeleri sıfırla.
- Beyaz çerçeveli bloklar = orijinal vokal notaları, ince beyaz çizgi = vokalin perde eğrisi.
- Tekerlek = yukarı/aşağı kaydır, **Ctrl + tekerlek** = zoom, **Shift + tekerlek** = yatay kaydır, **FIT** = hepsini sığdır.
- Düzenlemeler proje ile birlikte kaydedilir.

## Hazır harmoniler
3rd Up, 3rd Down, 5th Up, 5th Down, 4th Up, 6th Down, Octave Up, Octave Down, Triad (3rd + 5th), 3rd Up + 3rd Down,
Pop Stack, Four-Part Stack, Gospel Choir, Barbershop (Below), Open 4th + 5th, Power 5ths, Double Track, Wide Double + 3rd,
Dark Choir, Angel Choir, Trap Octaves, Demon Stack, Chipmunk, Robot Harmony.

## İpuçları
- En iyi sonuç: tek kişilik, kuru (reverbsiz) vokal. Reverb/efekti harmoniden sonra ekle.
- Bir harmoni notası kulağa yanlış geliyorsa (ör. akor değişiyor) o notayı sürükleyerek düzelt ya da sustur.

## Windows için derleme (GitHub)
1. Depoda **Actions** → "Build Windows VST3" her yüklemede kendiliğinden çalışır (~10 dk).
2. Bitince en altta **VonBerg-Harmony-VST3-Windows** dosyasını indir.
3. Zip'teki `VonBerg Harmony.vst3` klasörünü `C:\Program Files\Common Files\VST3\` içine kopyala.
4. FL Studio → Options → Manage plugins → Find plugins.

## Testler (Linux/Mac)
```
cmake -B build-test -DHARM_TESTS_ONLY=ON -DHARM_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-test
./build-test/EngineTest_artefacts/Release/EngineTest        # nota/ton algılama, harmoni perde doğruluğu
./build-test/PluginTest_artefacts/Release/PluginTest /tmp   # yükleme, preset, düzenleme, export, preview, REC, state
```
