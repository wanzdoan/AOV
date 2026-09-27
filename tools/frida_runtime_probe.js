'use strict';

const OFF = {
  actorManager: 0x8e355bc,
  getAllHeroes: 0x8faa7d0,
  isEnemyCamp: 0x8f98dec,
  setForceVisible: 0x8f993f8,
  refreshVisible: 0x8f92588,
  getPosition: 0x8f902b0,
  setPointerVisible: 0x86e7b24,
  fogEnable: 0x8e31db4,
  fogDisable: 0x8e31e28,
  getZoomRate: 0x76cb968,
  currentCamera: 0x77a4788,
  worldToScreen: 0x976cfb0,
};

function out(kind, data) {
  console.log(JSON.stringify({ kind, ...data }));
}

function safe(label, fn) {
  try {
    return fn();
  } catch (e) {
    out('error', { label, error: String(e) });
    return null;
  }
}

function readPtr(address) {
  return address.readPointer();
}

function pointerLooksReadable(address) {
  if (address === null || address.isNull()) return false;
  const range = Process.findRangeByAddress(address);
  return range !== null && range.protection.indexOf('r') !== -1;
}

function describeCode(base, name, rva) {
  safe('code:' + name, () => {
    const address = base.add(rva);
    const range = Process.findRangeByAddress(address);
    const bytes = Array.from(new Uint8Array(address.readByteArray(32)))
      .map(v => v.toString(16).padStart(2, '0')).join('');
    const instructions = [];
    let cursor = address;
    for (let i = 0; i < 6; i++) {
      const insn = Instruction.parse(cursor);
      instructions.push(insn.address.sub(base) + ': ' + insn.toString());
      cursor = insn.next;
    }
    out('code', {
      name,
      rva: '0x' + rva.toString(16),
      protection: range ? range.protection : null,
      bytes,
      instructions,
    });
  });
}

function inspectList(base, manager, fieldOffset, label) {
  safe('list:' + label, () => {
    const list = readPtr(manager.add(fieldOffset));
    const result = {
      label,
      fieldOffset: '0x' + fieldOffset.toString(16),
      list: list.toString(),
    };
    if (!pointerLooksReadable(list)) {
      result.readable = false;
      out('list', result);
      return;
    }
    result.readable = true;
    const items = readPtr(list.add(0x10));
    result.items = items.toString();
    result.size = list.add(0x18).readS32();
    result.version = list.add(0x1c).readS32();
    if (pointerLooksReadable(items)) {
      result.arrayLengthU64 = items.add(0x18).readU64().toString();
      result.arrayClass = readPtr(items).toString();
      result.elements = [];
      const limit = Math.min(Math.max(result.size, 0), 12);
      for (let i = 0; i < limit; i++) {
        const element = items.add(0x20 + i * 0x10);
        const seq = element.readU32();
        const object = readPtr(element.add(0x8));
        result.elements.push({
          i,
          seq,
          object: object.toString(),
          readable: pointerLooksReadable(object),
        });
      }
    }
    out('list', result);
  });
}

function installCounter(base, name, rva) {
  let count = 0;
  const address = base.add(rva);
  safe('hook:' + name, () => {
    Interceptor.attach(address, {
      onEnter(args) {
        count++;
        if (count <= 3) {
          out('call', {
            name,
            count,
            x0: args[0].toString(),
            x1: args[1].toString(),
            threadId: Process.getCurrentThreadId(),
          });
        }
      },
    });
  });
  return () => ({ name, count });
}

const il2cpp = Process.getModuleByName('libil2cpp.so');
out('module', {
  name: il2cpp.name,
  base: il2cpp.base.toString(),
  size: il2cpp.size,
  path: il2cpp.path,
});

for (const [name, rva] of Object.entries(OFF)) {
  describeCode(il2cpp.base, name, rva);
}

safe('call:actorManager', () => {
  // Static IL2CPP methods receive a hidden MethodInfo* as their only native
  // argument. Passing null mirrors generated internal call sites.
  const fn = new NativeFunction(il2cpp.base.add(OFF.actorManager),
    'pointer', ['pointer']);
  const manager = fn(ptr(0));
  out('actorManager', {
    pointer: manager.toString(),
    readable: pointerLooksReadable(manager),
  });
  if (!pointerLooksReadable(manager)) return;
  out('actorManagerHeader', {
    klass: readPtr(manager).toString(),
    monitor: readPtr(manager.add(Process.pointerSize)).toString(),
  });
  inspectList(il2cpp.base, manager, 0x18, 'GameActors');
  inspectList(il2cpp.base, manager, 0x20, 'HeroActors');
  inspectList(il2cpp.base, manager, 0x28, 'OrganActors');
});

safe('call:currentCamera', () => {
  const fn = new NativeFunction(il2cpp.base.add(OFF.currentCamera),
    'pointer', ['pointer']);
  const camera = fn(ptr(0));
  out('currentCamera', {
    pointer: camera.toString(),
    readable: pointerLooksReadable(camera),
  });
});

const counters = [
  installCounter(il2cpp.base, 'actorManager', OFF.actorManager),
  installCounter(il2cpp.base, 'refreshVisible', OFF.refreshVisible),
  installCounter(il2cpp.base, 'fogEnable', OFF.fogEnable),
  installCounter(il2cpp.base, 'fogDisable', OFF.fogDisable),
  installCounter(il2cpp.base, 'getZoomRate', OFF.getZoomRate),
  installCounter(il2cpp.base, 'currentCamera', OFF.currentCamera),
  installCounter(il2cpp.base, 'worldToScreen', OFF.worldToScreen),
];

setTimeout(() => {
  out('summary', { counters: counters.map(get => get()) });
}, 8000);
