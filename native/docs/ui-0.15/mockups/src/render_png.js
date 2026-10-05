// 0.15 stage 1, throwaway mockup code: draws every html file written by mockups.py to a PNG with Chromium (Playwright). usage: node render_png.js <html dir> <png dir>
const { chromium } = require('/opt/node22/lib/node_modules/playwright');
const fs = require('fs'), path = require('path');
(async () => {
  const [htmlDir, pngDir] = process.argv.slice(2);
  fs.mkdirSync(pngDir, { recursive: true });
  const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium', args: ['--allow-file-access-from-files'] });
  for (const f of fs.readdirSync(htmlDir).filter(n => n.endsWith('.html')).sort()) {
    const small = f.includes('_small');
    const page = await browser.newPage({ viewport: small ? { width: 1000, height: 640 } : { width: 1280, height: 800 } });
    await page.goto('file://' + path.resolve(htmlDir, f));
    await page.evaluate(() => document.fonts.ready);
    await page.screenshot({ path: path.join(pngDir, f.replace('.html', '.png')) });
    await page.close();
  }
  await browser.close();
})();
