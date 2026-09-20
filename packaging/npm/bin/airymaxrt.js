#!/usr/bin/env node
/*
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * AirymaxRT npm 安装通道薄壳（§12.12 N-2）
 *
 * 唯一职责：从发布面抓取与 curl / irm 通道字节一致的安装器附件，交棒执行，
 * 并把安装参数原样透传。包内不做平台探测、不解析 manifest、不解包、不落装
 * ——这些均属安装器（release 附件）的事实源范围。
 *
 * 网络目标有且仅有 RELEASE_BASE 下的 release 附件；安装根与 curl 通道同为
 * $HOME/.airymaxrt，安装产物布局不存在 npm 特例。
 */
'use strict';

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const https = require('node:https');
const { spawnSync } = require('node:child_process');

const RELEASE_BASE = 'https://atomgit.com/openairymax/agentrt/releases/download';
const MAX_ASSET = 4 * 1024 * 1024;
const WIN = process.platform === 'win32';

const ASSET = WIN ? 'install.ps1' : 'install.sh';
const LAUNCHER = WIN ? 'airymaxrt.cmd' : 'airymaxrt';

// 安装意图类参数：始终由安装器处置，不做运行期委派。
const INSTALL_FLAGS = ['--prefix', '--reinstall', '--uninstall'];

// PowerShell 参数语法与安装器长选项同名异写：只做拼写映射，不改语义。
const PS_VALUE = {
  '--prefix': '-Prefix',
  '--mode': '-Mode',
  '--bin-dir': '-BinDir',
  '--channel': '-Channel',
  '--from-file': '-FromFile',
};
const PS_SWITCH = {
  '--uninstall': '-Uninstall',
  '--keep-data': '-KeepData',
  '--yes': '-Yes',
  '--help': '-Help',
};
const PS_DROP = ['--reinstall']; // 重跑安装器即幂等重装，Windows 侧无此开关

function die(msg) {
  process.stderr.write(`[FAIL] ${msg}\n`);
  process.exit(1);
}

function wantsInstall(argv) {
  return argv.some((a) => INSTALL_FLAGS.includes(a));
}

// 运行期入口按「N-3 默认根 → PATH」顺序定位，安装根判定仍归安装器（薄壳只查
// 既有产物是否就位）。isSelf 排除本 shim：POSIX 比符号链目标，Windows 的 npm
// shim 是独立 .cmd 文件，按其中内嵌的本包路径识别，防自我委派死循环。
function isSelf(p) {
  try {
    if (fs.realpathSync(p) === fs.realpathSync(__filename)) return true;
  } catch {
    /* 不可解析：非本包 */
  }
  if (!WIN) return false;
  try {
    const text = fs.readFileSync(p, 'utf8');
    return text.includes(__dirname) || text.includes(__filename);
  } catch {
    return false;
  }
}

function runtime() {
  const cands = [path.join(os.homedir(), '.airymaxrt', 'bin', LAUNCHER)];
  for (const dir of (process.env.PATH || '').split(path.delimiter)) {
    if (dir) cands.push(path.join(dir, LAUNCHER));
  }
  return cands.find((p) => fs.existsSync(p) && !isSelf(p)) || null;
}

// 值型开关连同其后的实参一并搬运，其余参数不识别则原样交给安装器报错。
function toWindows(argv) {
  const out = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (PS_DROP.includes(a)) continue;
    const sw = PS_SWITCH[a];
    if (sw) {
      out.push(sw);
      continue;
    }
    const val = PS_VALUE[a];
    if (val) {
      const v = argv[++i];
      if (v === undefined) die(`${a} 缺少取值`);
      out.push(val, v);
      continue;
    }
    out.push(a);
  }
  return out;
}

function grab(url, hops) {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { headers: { 'user-agent': 'airymaxrt-npm-channel' } }, (res) => {
      if ([301, 302, 303, 307, 308].includes(res.statusCode)) {
        const loc = res.headers.location;
        res.resume();
        if (!loc) {
          reject(new Error('重定向缺少 Location'));
          return;
        }
        if (hops <= 0) {
          reject(new Error('重定向次数超限'));
          return;
        }
        grab(new URL(loc, url).href, hops - 1).then(resolve, reject);
        return;
      }
      if (res.statusCode !== 200) {
        res.resume();
        reject(new Error(`HTTP ${res.statusCode}`));
        return;
      }
      const parts = [];
      let size = 0;
      res.on('data', (c) => {
        size += c.length;
        if (size > MAX_ASSET) {
          res.destroy();
          reject(new Error('附件尺寸异常'));
          return;
        }
        parts.push(c);
      });
      res.on('end', () => resolve(Buffer.concat(parts)));
      res.on('error', reject);
    });
    req.setTimeout(60000, () => req.destroy(new Error('拉取超时（60s）')));
    req.on('error', reject);
  });
}

function handoff(body, argv) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'airymaxrt-'));
  const file = path.join(dir, ASSET);
  fs.writeFileSync(file, body, { mode: 0o600 });
  const pre = WIN
    ? ['powershell', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', file]]
    : ['bash', [file]];
  const r = spawnSync(pre[0], [...pre[1], ...(WIN ? toWindows(argv) : argv)], { stdio: 'inherit' });
  fs.rmSync(dir, { recursive: true, force: true });
  if (r.error) die(`安装器执行失败: ${r.error.message}`);
  if (r.signal) die(`安装器被信号终止: ${r.signal}`);
  return r.status;
}

async function main() {
  const argv = process.argv.slice(2);
  const rt = wantsInstall(argv) ? null : runtime();
  if (rt) {
    const host = WIN ? ['cmd.exe', ['/d', '/s', '/c', rt]] : [rt, []];
    const r = spawnSync(host[0], [...host[1], ...argv], { stdio: 'inherit' });
    if (r.error) die(`启动器不可用: ${r.error.message}`);
    return r.status === null ? 1 : r.status;
  }
  const url = `${RELEASE_BASE}/latest/${ASSET}`;
  let body;
  try {
    body = await grab(url, 5);
  } catch (e) {
    die(`附件拉取失败 ${url}: ${e.message}`);
  }
  if (body.length === 0) die(`附件为空: ${url}`);
  return handoff(body, argv);
}

module.exports = { wantsInstall, toWindows };

if (require.main === module) {
  main().then(
    (code) => {
      process.exitCode = code === null ? 1 : code;
    },
    (e) => die(e.message),
  );
}
