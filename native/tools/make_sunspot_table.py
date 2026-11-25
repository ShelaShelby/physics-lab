# 0.85.0 (PLAN_REVIEW.md R3, the owner's answer of 25 Nov 2026: the packed table is kept in this repository): the measured sunspot groups, one row per group
# per day, packed for the Orbit Lab's Sun (src/orbit/sunspots.h reads it). Only the standard library.
#   python3 native/tools/make_sunspot_table.py <sources checkout> native/data/sun/spots/sunspot_groups.bin
# (the SRS files are read from <sources>/data/sun/spots/noaa_srs_2016-09-25_2026-11-24.tar.gz: every daily file fetched from NCEI on 2026-11-25, as received)
# Inputs (all saved in the private sources repository, native/data/SOURCES.md):
#  (1) the Royal Greenwich Observatory - USAF/NOAA sunspot group records from NASA Marshall (D. Hathaway): data/sun/spots/RGO_NOAA1874_2013.zip (g1874.txt ...
#      g2013.txt) and g2014.txt, g2015.txt, g2016.txt; the columns of format.txt; the days of MissingDays.txt (RGO, 1874-1976).
#      Used: the date, the corrected whole-spot area (millionths of the solar hemisphere, columns 41-44), the Carrington longitude (58-62) and the latitude (64-68).
#      The raw files are used as they are: NASA's derived data raise the USAF/NOAA areas after 1976 by 1.4 (msfc_greenwch_page); the app does not (it says so).
#  (2) NOAA SWPC's daily Solar Region Summary (SRS), archived by NOAA NCEI, from 3 Oct 2016: part I, "Regions with Sunspots": the location (latitude),
#      Lo (Carrington longitude) and Area (millionths of the hemisphere). A report dated D gives the regions at 00:00 UT of D. SWPC calls these preliminary data.
# Rows with an area of 0 are left out (nothing to draw). Every row's day is checked against its file; the rows per year are printed.
# Output (little endian): "PLSPOTS1", uint32 rows, uint32 missing days, uint32 first day, uint32 last day, uint32 the first SRS day, uint32 the first day whose rows are
# at 00:00 UT (1992-01-02; before it a row is at 12:00 UT of its day), then the rows sorted by day
# (uint16 day, int16 latitude x 10, uint16 Carrington longitude x 10, uint16 area), then the missing days (uint16). A day is counted from 1874-01-01 (day 0).
import datetime, io, os, re, struct, sys, tarfile, zipfile, collections

src, out = sys.argv[1], sys.argv[2]
spots = os.path.join(src, 'data', 'sun', 'spots')
with tarfile.open(os.path.join(spots, 'noaa_srs_2016-09-25_2026-11-24.tar.gz')) as tf:
    srsFiles = {m.name: tf.extractfile(m).read().decode('ascii', errors='replace') for m in tf.getmembers() if m.isfile()}
D0 = datetime.date(1874, 1, 1)
def day(y, m, d): return (datetime.date(y, m, d) - D0).days

rows, perYear, over360, gAll = [], collections.Counter(), [], [0]
def gfile(name, text):
    y0 = int(re.search(r'g(\d{4})', name).group(1))
    for line in text.splitlines():
        if len(line.rstrip()) < 74: continue
        y, mo, d = int(line[0:4]), int(line[4:6]), int(line[6:8])
        assert y == y0, (name, line)
        area = int(line[40:44]); lon = float(line[57:62]); lat = float(line[63:68])
        perYear[y] += 1
        gAll[0] += 1
        if area <= 0: continue
        assert -90 <= lat <= 90 and 0 <= lon < 720, line
        if lon >= 360: over360.append(line)   # (some rows print 360.1 to 361.x: taken modulo 360 and counted)
        # From 1992 the rows' places (distance, position angle, central meridian distance, Carrington longitude) match 24:00 UT of the printed day, not
        # its printed .500 (measured on each year's file in 0.85.0: REPORT.md): they are stored as 00:00 UT of the next day, as the SRS rows are.
        rows.append((day(y, mo, d) + (1 if y >= 1992 else 0), int(round(lat * 10)), int(round(lon * 10)) % 3600, area))
z = zipfile.ZipFile(os.path.join(spots, 'RGO_NOAA1874_2013.zip'))
for n in sorted(z.namelist()):
    if re.match(r'g\d{4}\.txt$', n): gfile(n, z.read(n).decode('ascii'))
for y in (2014, 2015, 2016):
    gfile('g%d.txt' % y, open(os.path.join(spots, 'g%d.txt' % y), encoding='ascii').read())
lastG = max(r[0] for r in rows)

missing = []
for line in open(os.path.join(spots, 'MissingDays.txt'), encoding='ascii'):
    m = re.match(r'^(\d{4})([ \d]\d)([ \d]\d)\s*$', line.rstrip('\n'))
    if m: missing.append(day(int(m.group(1)), int(m.group(2)), int(m.group(3))))

srsFirst = lastG + 1
d = srsFirst
srsRows = 0
emptyFiles = []
unread = []
lastDay = d
while True:
    date = D0 + datetime.timedelta(days=d)
    f = date.strftime('%Y%m%d') + 'SRS.txt'
    if date > datetime.date(2026, 11, 25): break
    if f not in srsFiles: missing.append(d); d += 1; continue
    text = srsFiles[f]
    if 'Regions with Sunspots' not in text or 'IA.' not in text: missing.append(d); emptyFiles.append(f); d += 1; continue   # (an empty or cut file: no record)
    v = re.search(r'Locations Valid at (\d\d)/2400Z', text)   # the regions at 24:00 UT of the day before the file's date (00:00 UT of its date)
    assert v and int(v.group(1)) == (date - datetime.timedelta(days=1)).day, f
    part = text[text.index('I.  Regions with Sunspots'):text.index('IA.')]
    for line in part.splitlines()[2:]:
        m = re.match(r'^\s*(\d+)\s+([NS])(\d\d)([EW])(\S*)\s+(\d{3})\s+(\d{4})\s', line)   # (the central meridian distance is not used: one file prints W0*)
        if not m:
            if line.strip() not in ('None', '') and not line.startswith('Nmbr'): unread.append(os.path.basename(f) + ': ' + line.strip())
            continue
        lat = int(m.group(3)) * (1 if m.group(2) == 'N' else -1)
        area = int(m.group(7))
        perYear[date.year] += 1
        if area <= 0: continue
        rows.append((d, lat * 10, int(m.group(6)) * 10 % 3600, area)); srsRows += 1
    lastDay = d
    d += 1
while missing and missing[-1] > lastDay: missing.pop()
rows.sort()
first = rows[0][0]
missing = sorted(set(m for m in missing if first <= m <= lastDay))
with open(out, 'wb') as fo:
    fo.write(b'PLSPOTS1' + struct.pack('<6I', len(rows), len(missing), first, lastDay, srsFirst, day(1992, 1, 2)))
    for r in rows: fo.write(struct.pack('<HhHH', *r))
    for m in missing: fo.write(struct.pack('<H', m))
print('rows %d (SRS %d), missing days %d, first %s, last %s, first SRS day %s' % (len(rows), srsRows, len(missing), D0 + datetime.timedelta(days=first),
      D0 + datetime.timedelta(days=lastDay), D0 + datetime.timedelta(days=srsFirst)))
print('Greenwich and USAF/NOAA rows 1874-2016: %d, %d with an area above 0' % (gAll[0], len(rows) - srsRows))
print('SRS files without part I (counted as no record): %d %s' % (len(emptyFiles), [os.path.basename(x) for x in emptyFiles][:10]))
print('SRS lines not read: %d %s' % (len(unread), unread[:10]))
print('Carrington longitude >= 360 in %d rows (taken modulo 360), e.g.: %s' % (len(over360), over360[:2]))
print('rows per year (all, with area 0):', ' '.join('%d:%d' % (y, perYear[y]) for y in sorted(perYear)))
