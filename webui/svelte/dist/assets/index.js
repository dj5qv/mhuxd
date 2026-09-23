var __defProp = Object.defineProperty;
var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);
(function polyfill() {
  const relList = document.createElement("link").relList;
  if (relList && relList.supports && relList.supports("modulepreload")) {
    return;
  }
  for (const link of document.querySelectorAll('link[rel="modulepreload"]')) {
    processPreload(link);
  }
  new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      if (mutation.type !== "childList") {
        continue;
      }
      for (const node of mutation.addedNodes) {
        if (node.tagName === "LINK" && node.rel === "modulepreload")
          processPreload(node);
      }
    }
  }).observe(document, { childList: true, subtree: true });
  function getFetchOpts(link) {
    const fetchOpts = {};
    if (link.integrity) fetchOpts.integrity = link.integrity;
    if (link.referrerPolicy) fetchOpts.referrerPolicy = link.referrerPolicy;
    if (link.crossOrigin === "use-credentials")
      fetchOpts.credentials = "include";
    else if (link.crossOrigin === "anonymous") fetchOpts.credentials = "omit";
    else fetchOpts.credentials = "same-origin";
    return fetchOpts;
  }
  function processPreload(link) {
    if (link.ep)
      return;
    link.ep = true;
    const fetchOpts = getFetchOpts(link);
    fetch(link.href, fetchOpts);
  }
})();
function noop() {
}
function run(fn) {
  return fn();
}
function blank_object() {
  return /* @__PURE__ */ Object.create(null);
}
function run_all(fns) {
  fns.forEach(run);
}
function is_function(thing) {
  return typeof thing === "function";
}
function safe_not_equal(a, b) {
  return a != a ? b == b : a !== b || a && typeof a === "object" || typeof a === "function";
}
function is_empty(obj) {
  return Object.keys(obj).length === 0;
}
function subscribe(store, ...callbacks) {
  if (store == null) {
    for (const callback of callbacks) {
      callback(void 0);
    }
    return noop;
  }
  const unsub = store.subscribe(...callbacks);
  return unsub.unsubscribe ? () => unsub.unsubscribe() : unsub;
}
function component_subscribe(component, store, callback) {
  component.$$.on_destroy.push(subscribe(store, callback));
}
function null_to_empty(value) {
  return value == null ? "" : value;
}
function append(target, node) {
  target.appendChild(node);
}
function insert(target, node, anchor) {
  target.insertBefore(node, anchor || null);
}
function detach(node) {
  if (node.parentNode) {
    node.parentNode.removeChild(node);
  }
}
function destroy_each(iterations, detaching) {
  for (let i = 0; i < iterations.length; i += 1) {
    if (iterations[i]) iterations[i].d(detaching);
  }
}
function element(name) {
  return document.createElement(name);
}
function text(data) {
  return document.createTextNode(data);
}
function space() {
  return text(" ");
}
function empty() {
  return text("");
}
function listen(node, event, handler, options) {
  node.addEventListener(event, handler, options);
  return () => node.removeEventListener(event, handler, options);
}
function self(fn) {
  return function(event) {
    if (event.target === this) fn.call(this, event);
  };
}
function attr(node, attribute, value) {
  if (value == null) node.removeAttribute(attribute);
  else if (node.getAttribute(attribute) !== value) node.setAttribute(attribute, value);
}
function init_binding_group(group) {
  let _inputs;
  return {
    /* push */
    p(...inputs) {
      _inputs = inputs;
      _inputs.forEach((input) => group.push(input));
    },
    /* remove */
    r() {
      _inputs.forEach((input) => group.splice(group.indexOf(input), 1));
    }
  };
}
function to_number(value) {
  return value === "" ? null : +value;
}
function children(element2) {
  return Array.from(element2.childNodes);
}
function set_data(text2, data) {
  data = "" + data;
  if (text2.data === data) return;
  text2.data = /** @type {string} */
  data;
}
function set_input_value(input, value) {
  input.value = value == null ? "" : value;
}
function set_style(node, key, value, important) {
  if (value == null) {
    node.style.removeProperty(key);
  } else {
    node.style.setProperty(key, value, "");
  }
}
function select_option(select, value, mounting) {
  for (let i = 0; i < select.options.length; i += 1) {
    const option = select.options[i];
    if (option.__value === value) {
      option.selected = true;
      return;
    }
  }
  if (!mounting || value !== void 0) {
    select.selectedIndex = -1;
  }
}
function select_value(select) {
  const selected_option = select.querySelector(":checked");
  return selected_option && selected_option.__value;
}
function custom_event(type, detail, { bubbles = false, cancelable = false } = {}) {
  return new CustomEvent(type, { detail, bubbles, cancelable });
}
let current_component;
function set_current_component(component) {
  current_component = component;
}
function get_current_component() {
  if (!current_component) throw new Error("Function called outside component initialization");
  return current_component;
}
function onMount(fn) {
  get_current_component().$$.on_mount.push(fn);
}
function onDestroy(fn) {
  get_current_component().$$.on_destroy.push(fn);
}
function createEventDispatcher() {
  const component = get_current_component();
  return (type, detail, { cancelable = false } = {}) => {
    const callbacks = component.$$.callbacks[type];
    if (callbacks) {
      const event = custom_event(
        /** @type {string} */
        type,
        detail,
        { cancelable }
      );
      callbacks.slice().forEach((fn) => {
        fn.call(component, event);
      });
      return !event.defaultPrevented;
    }
    return true;
  };
}
const dirty_components = [];
const binding_callbacks = [];
let render_callbacks = [];
const flush_callbacks = [];
const resolved_promise = /* @__PURE__ */ Promise.resolve();
let update_scheduled = false;
function schedule_update() {
  if (!update_scheduled) {
    update_scheduled = true;
    resolved_promise.then(flush);
  }
}
function add_render_callback(fn) {
  render_callbacks.push(fn);
}
function add_flush_callback(fn) {
  flush_callbacks.push(fn);
}
const seen_callbacks = /* @__PURE__ */ new Set();
let flushidx = 0;
function flush() {
  if (flushidx !== 0) {
    return;
  }
  const saved_component = current_component;
  do {
    try {
      while (flushidx < dirty_components.length) {
        const component = dirty_components[flushidx];
        flushidx++;
        set_current_component(component);
        update(component.$$);
      }
    } catch (e) {
      dirty_components.length = 0;
      flushidx = 0;
      throw e;
    }
    set_current_component(null);
    dirty_components.length = 0;
    flushidx = 0;
    while (binding_callbacks.length) binding_callbacks.pop()();
    for (let i = 0; i < render_callbacks.length; i += 1) {
      const callback = render_callbacks[i];
      if (!seen_callbacks.has(callback)) {
        seen_callbacks.add(callback);
        callback();
      }
    }
    render_callbacks.length = 0;
  } while (dirty_components.length);
  while (flush_callbacks.length) {
    flush_callbacks.pop()();
  }
  update_scheduled = false;
  seen_callbacks.clear();
  set_current_component(saved_component);
}
function update($$) {
  if ($$.fragment !== null) {
    $$.update();
    run_all($$.before_update);
    const dirty = $$.dirty;
    $$.dirty = [-1];
    $$.fragment && $$.fragment.p($$.ctx, dirty);
    $$.after_update.forEach(add_render_callback);
  }
}
function flush_render_callbacks(fns) {
  const filtered = [];
  const targets = [];
  render_callbacks.forEach((c) => fns.indexOf(c) === -1 ? filtered.push(c) : targets.push(c));
  targets.forEach((c) => c());
  render_callbacks = filtered;
}
const outroing = /* @__PURE__ */ new Set();
let outros;
function group_outros() {
  outros = {
    r: 0,
    c: [],
    p: outros
    // parent group
  };
}
function check_outros() {
  if (!outros.r) {
    run_all(outros.c);
  }
  outros = outros.p;
}
function transition_in(block, local) {
  if (block && block.i) {
    outroing.delete(block);
    block.i(local);
  }
}
function transition_out(block, local, detach2, callback) {
  if (block && block.o) {
    if (outroing.has(block)) return;
    outroing.add(block);
    outros.c.push(() => {
      outroing.delete(block);
      if (callback) {
        if (detach2) block.d(1);
        callback();
      }
    });
    block.o(local);
  } else if (callback) {
    callback();
  }
}
function ensure_array_like(array_like_or_iterator) {
  return (array_like_or_iterator == null ? void 0 : array_like_or_iterator.length) !== void 0 ? array_like_or_iterator : Array.from(array_like_or_iterator);
}
function destroy_block(block, lookup) {
  block.d(1);
  lookup.delete(block.key);
}
function update_keyed_each(old_blocks, dirty, get_key, dynamic, ctx, list, lookup, node, destroy, create_each_block2, next, get_context) {
  let o = old_blocks.length;
  let n = list.length;
  let i = o;
  const old_indexes = {};
  while (i--) old_indexes[old_blocks[i].key] = i;
  const new_blocks = [];
  const new_lookup = /* @__PURE__ */ new Map();
  const deltas = /* @__PURE__ */ new Map();
  const updates = [];
  i = n;
  while (i--) {
    const child_ctx = get_context(ctx, list, i);
    const key = get_key(child_ctx);
    let block = lookup.get(key);
    if (!block) {
      block = create_each_block2(key, child_ctx);
      block.c();
    } else {
      updates.push(() => block.p(child_ctx, dirty));
    }
    new_lookup.set(key, new_blocks[i] = block);
    if (key in old_indexes) deltas.set(key, Math.abs(i - old_indexes[key]));
  }
  const will_move = /* @__PURE__ */ new Set();
  const did_move = /* @__PURE__ */ new Set();
  function insert2(block) {
    transition_in(block, 1);
    block.m(node, next);
    lookup.set(block.key, block);
    next = block.first;
    n--;
  }
  while (o && n) {
    const new_block = new_blocks[n - 1];
    const old_block = old_blocks[o - 1];
    const new_key = new_block.key;
    const old_key = old_block.key;
    if (new_block === old_block) {
      next = new_block.first;
      o--;
      n--;
    } else if (!new_lookup.has(old_key)) {
      destroy(old_block, lookup);
      o--;
    } else if (!lookup.has(new_key) || will_move.has(new_key)) {
      insert2(new_block);
    } else if (did_move.has(old_key)) {
      o--;
    } else if (deltas.get(new_key) > deltas.get(old_key)) {
      did_move.add(new_key);
      insert2(new_block);
    } else {
      will_move.add(old_key);
      o--;
    }
  }
  while (o--) {
    const old_block = old_blocks[o];
    if (!new_lookup.has(old_block.key)) destroy(old_block, lookup);
  }
  while (n) insert2(new_blocks[n - 1]);
  run_all(updates);
  return new_blocks;
}
function bind(component, name, callback) {
  const index = component.$$.props[name];
  if (index !== void 0) {
    component.$$.bound[index] = callback;
    callback(component.$$.ctx[index]);
  }
}
function create_component(block) {
  block && block.c();
}
function mount_component(component, target, anchor) {
  const { fragment, after_update } = component.$$;
  fragment && fragment.m(target, anchor);
  add_render_callback(() => {
    const new_on_destroy = component.$$.on_mount.map(run).filter(is_function);
    if (component.$$.on_destroy) {
      component.$$.on_destroy.push(...new_on_destroy);
    } else {
      run_all(new_on_destroy);
    }
    component.$$.on_mount = [];
  });
  after_update.forEach(add_render_callback);
}
function destroy_component(component, detaching) {
  const $$ = component.$$;
  if ($$.fragment !== null) {
    flush_render_callbacks($$.after_update);
    run_all($$.on_destroy);
    $$.fragment && $$.fragment.d(detaching);
    $$.on_destroy = $$.fragment = null;
    $$.ctx = [];
  }
}
function make_dirty(component, i) {
  if (component.$$.dirty[0] === -1) {
    dirty_components.push(component);
    schedule_update();
    component.$$.dirty.fill(0);
  }
  component.$$.dirty[i / 31 | 0] |= 1 << i % 31;
}
function init(component, options, instance2, create_fragment2, not_equal, props, append_styles = null, dirty = [-1]) {
  const parent_component = current_component;
  set_current_component(component);
  const $$ = component.$$ = {
    fragment: null,
    ctx: [],
    // state
    props,
    update: noop,
    not_equal,
    bound: blank_object(),
    // lifecycle
    on_mount: [],
    on_destroy: [],
    on_disconnect: [],
    before_update: [],
    after_update: [],
    context: new Map(options.context || (parent_component ? parent_component.$$.context : [])),
    // everything else
    callbacks: blank_object(),
    dirty,
    skip_bound: false,
    root: options.target || parent_component.$$.root
  };
  append_styles && append_styles($$.root);
  let ready = false;
  $$.ctx = instance2 ? instance2(component, options.props || {}, (i, ret, ...rest) => {
    const value = rest.length ? rest[0] : ret;
    if ($$.ctx && not_equal($$.ctx[i], $$.ctx[i] = value)) {
      if (!$$.skip_bound && $$.bound[i]) $$.bound[i](value);
      if (ready) make_dirty(component, i);
    }
    return ret;
  }) : [];
  $$.update();
  ready = true;
  run_all($$.before_update);
  $$.fragment = create_fragment2 ? create_fragment2($$.ctx) : false;
  if (options.target) {
    if (options.hydrate) {
      const nodes = children(options.target);
      $$.fragment && $$.fragment.l(nodes);
      nodes.forEach(detach);
    } else {
      $$.fragment && $$.fragment.c();
    }
    if (options.intro) transition_in(component.$$.fragment);
    mount_component(component, options.target, options.anchor);
    flush();
  }
  set_current_component(parent_component);
}
class SvelteComponent {
  constructor() {
    /**
     * ### PRIVATE API
     *
     * Do not use, may change at any time
     *
     * @type {any}
     */
    __publicField(this, "$$");
    /**
     * ### PRIVATE API
     *
     * Do not use, may change at any time
     *
     * @type {any}
     */
    __publicField(this, "$$set");
  }
  /** @returns {void} */
  $destroy() {
    destroy_component(this, 1);
    this.$destroy = noop;
  }
  /**
   * @template {Extract<keyof Events, string>} K
   * @param {K} type
   * @param {((e: Events[K]) => void) | null | undefined} callback
   * @returns {() => void}
   */
  $on(type, callback) {
    if (!is_function(callback)) {
      return noop;
    }
    const callbacks = this.$$.callbacks[type] || (this.$$.callbacks[type] = []);
    callbacks.push(callback);
    return () => {
      const index = callbacks.indexOf(callback);
      if (index !== -1) callbacks.splice(index, 1);
    };
  }
  /**
   * @param {Partial<Props>} props
   * @returns {void}
   */
  $set(props) {
    if (this.$$set && !is_empty(props)) {
      this.$$.skip_bound = true;
      this.$$set(props);
      this.$$.skip_bound = false;
    }
  }
}
const PUBLIC_VERSION = "4";
if (typeof window !== "undefined")
  (window.__svelte || (window.__svelte = { v: /* @__PURE__ */ new Set() })).v.add(PUBLIC_VERSION);
const subscriber_queue = [];
function writable(value, start = noop) {
  let stop;
  const subscribers = /* @__PURE__ */ new Set();
  function set(new_value) {
    if (safe_not_equal(value, new_value)) {
      value = new_value;
      if (stop) {
        const run_queue = !subscriber_queue.length;
        for (const subscriber of subscribers) {
          subscriber[1]();
          subscriber_queue.push(subscriber, value);
        }
        if (run_queue) {
          for (let i = 0; i < subscriber_queue.length; i += 2) {
            subscriber_queue[i][0](subscriber_queue[i + 1]);
          }
          subscriber_queue.length = 0;
        }
      }
    }
  }
  function update2(fn) {
    set(fn(value));
  }
  function subscribe2(run2, invalidate = noop) {
    const subscriber = [run2, invalidate];
    subscribers.add(subscriber);
    if (subscribers.size === 1) {
      stop = start(set, update2) || noop;
    }
    run2(value);
    return () => {
      subscribers.delete(subscriber);
      if (subscribers.size === 0 && stop) {
        stop();
        stop = null;
      }
    };
  }
  return { set, update: update2, subscribe: subscribe2 };
}
const wsConnected = writable(true);
const wsRetrySeconds = writable(0);
let socket = null;
let reconnectTimer = null;
const listeners = /* @__PURE__ */ new Map();
let onOpenCallback = null;
function wsUrl() {
  const loc = window.location;
  const proto = loc.protocol === "https:" ? "wss:" : "ws:";
  return `${proto}//${loc.host}/api/v1/ws`;
}
function scheduleReconnect() {
  const delay = 5;
  let seconds = delay;
  wsRetrySeconds.set(seconds);
  if (reconnectTimer) clearInterval(reconnectTimer);
  reconnectTimer = setInterval(() => {
    seconds -= 1;
    wsRetrySeconds.set(seconds);
    if (seconds <= 0) {
      clearInterval(reconnectTimer);
      reconnectTimer = null;
      connect();
    }
  }, 1e3);
}
function connect() {
  if (socket) {
    socket.onclose = null;
    socket.close();
  }
  socket = new WebSocket(wsUrl());
  socket.onopen = () => {
    wsConnected.set(true);
    wsRetrySeconds.set(0);
    if (reconnectTimer) {
      clearInterval(reconnectTimer);
      reconnectTimer = null;
    }
    if (onOpenCallback) onOpenCallback();
  };
  socket.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      const type = data.type || "message";
      const typeHandlers = listeners.get(type);
      if (typeHandlers) typeHandlers.forEach((fn) => fn(data));
      const wildcard = listeners.get("*");
      if (wildcard) wildcard.forEach((fn) => fn(data));
    } catch (e) {
      console.error("Failed to parse WS message", e);
    }
  };
  socket.onerror = (err) => {
    console.error("WebSocket error:", err);
  };
  socket.onclose = () => {
    wsConnected.set(false);
    scheduleReconnect();
  };
}
function connectWs(onOpen) {
  onOpenCallback = onOpen || null;
  connect();
}
function disconnectWs() {
  if (reconnectTimer) {
    clearInterval(reconnectTimer);
    reconnectTimer = null;
  }
  if (socket) {
    socket.onclose = null;
    socket.close();
    socket = null;
  }
  wsConnected.set(false);
  wsRetrySeconds.set(0);
}
function onWsEvent(type, handler) {
  if (!listeners.has(type)) listeners.set(type, /* @__PURE__ */ new Set());
  listeners.get(type).add(handler);
  return () => {
    const set = listeners.get(type);
    if (set) {
      set.delete(handler);
      if (set.size === 0) listeners.delete(type);
    }
  };
}
function wsSend(msg) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(JSON.stringify(msg));
    return true;
  }
  return false;
}
function create_if_block$6(ctx) {
  let div1;
  let div0;
  let t0;
  let t1;
  let t2;
  let t3_value = (
    /*$wsRetrySeconds*/
    ctx[1] === 1 ? "second" : "seconds"
  );
  let t3;
  return {
    c() {
      div1 = element("div");
      div0 = element("div");
      t0 = text("Can't connect to mhuxd. Retry in ");
      t1 = text(
        /*$wsRetrySeconds*/
        ctx[1]
      );
      t2 = space();
      t3 = text(t3_value);
      attr(div0, "class", "connection-message");
      attr(div1, "class", "connection-overlay");
    },
    m(target, anchor) {
      insert(target, div1, anchor);
      append(div1, div0);
      append(div0, t0);
      append(div0, t1);
      append(div0, t2);
      append(div0, t3);
    },
    p(ctx2, dirty) {
      if (dirty & /*$wsRetrySeconds*/
      2) set_data(
        t1,
        /*$wsRetrySeconds*/
        ctx2[1]
      );
      if (dirty & /*$wsRetrySeconds*/
      2 && t3_value !== (t3_value = /*$wsRetrySeconds*/
      ctx2[1] === 1 ? "second" : "seconds")) set_data(t3, t3_value);
    },
    d(detaching) {
      if (detaching) {
        detach(div1);
      }
    }
  };
}
function create_fragment$9(ctx) {
  let if_block_anchor;
  let if_block = !/*$wsConnected*/
  ctx[0] && create_if_block$6(ctx);
  return {
    c() {
      if (if_block) if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if (if_block) if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, [dirty]) {
      if (!/*$wsConnected*/
      ctx2[0]) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block$6(ctx2);
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if (if_block) if_block.d(detaching);
    }
  };
}
function instance$9($$self, $$props, $$invalidate) {
  let $wsConnected;
  let $wsRetrySeconds;
  component_subscribe($$self, wsConnected, ($$value) => $$invalidate(0, $wsConnected = $$value));
  component_subscribe($$self, wsRetrySeconds, ($$value) => $$invalidate(1, $wsRetrySeconds = $$value));
  return [$wsConnected, $wsRetrySeconds];
}
class ConnectionOverlay extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$9, create_fragment$9, safe_not_equal, {});
  }
}
function create_fragment$8(ctx) {
  let header;
  let div0;
  let t2;
  let div2;
  let div1;
  let t3;
  let t4;
  let t5;
  let t6;
  let t7;
  let a;
  return {
    c() {
      header = element("header");
      div0 = element("div");
      div0.innerHTML = `<span class="title-italic">m</span>huxd Device Router`;
      t2 = space();
      div2 = element("div");
      div1 = element("div");
      t3 = text("Hostname: ");
      t4 = text(
        /*hostname*/
        ctx[0]
      );
      t5 = text(" | ");
      t6 = text(
        /*version*/
        ctx[1]
      );
      t7 = space();
      a = element("a");
      a.textContent = "Help";
      attr(div0, "class", "title");
      attr(div1, "class", "hostname");
      attr(a, "class", "help");
      attr(a, "href", "http://mhuxd.dj5qv.de/doc/");
      attr(a, "target", "_blank");
      attr(a, "rel", "noreferrer");
      attr(div2, "class", "top-right");
      attr(header, "class", "topbar");
    },
    m(target, anchor) {
      insert(target, header, anchor);
      append(header, div0);
      append(header, t2);
      append(header, div2);
      append(div2, div1);
      append(div1, t3);
      append(div1, t4);
      append(div1, t5);
      append(div1, t6);
      append(div2, t7);
      append(div2, a);
    },
    p(ctx2, [dirty]) {
      if (dirty & /*hostname*/
      1) set_data(
        t4,
        /*hostname*/
        ctx2[0]
      );
      if (dirty & /*version*/
      2) set_data(
        t6,
        /*version*/
        ctx2[1]
      );
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(header);
      }
    }
  };
}
function instance$8($$self, $$props, $$invalidate) {
  let { hostname = "—" } = $$props;
  let { version = "—" } = $$props;
  $$self.$$set = ($$props2) => {
    if ("hostname" in $$props2) $$invalidate(0, hostname = $$props2.hostname);
    if ("version" in $$props2) $$invalidate(1, version = $$props2.version);
  };
  return [hostname, version];
}
class TopBar extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$8, create_fragment$8, safe_not_equal, { hostname: 0, version: 1 });
  }
}
function create_fragment$7(ctx) {
  let span;
  let span_class_value;
  return {
    c() {
      span = element("span");
      attr(span, "class", span_class_value = "status-dot " + /*statusClass*/
      ctx[1] + " svelte-bogpxu");
      attr(
        span,
        "title",
        /*status*/
        ctx[0]
      );
      attr(
        span,
        "aria-label",
        /*status*/
        ctx[0]
      );
    },
    m(target, anchor) {
      insert(target, span, anchor);
    },
    p(ctx2, [dirty]) {
      if (dirty & /*statusClass*/
      2 && span_class_value !== (span_class_value = "status-dot " + /*statusClass*/
      ctx2[1] + " svelte-bogpxu")) {
        attr(span, "class", span_class_value);
      }
      if (dirty & /*status*/
      1) {
        attr(
          span,
          "title",
          /*status*/
          ctx2[0]
        );
      }
      if (dirty & /*status*/
      1) {
        attr(
          span,
          "aria-label",
          /*status*/
          ctx2[0]
        );
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(span);
      }
    }
  };
}
function instance$7($$self, $$props, $$invalidate) {
  let s;
  let statusClass;
  let { status = "UNKNOWN" } = $$props;
  $$self.$$set = ($$props2) => {
    if ("status" in $$props2) $$invalidate(0, status = $$props2.status);
  };
  $$self.$$.update = () => {
    if ($$self.$$.dirty & /*status*/
    1) {
      $$invalidate(2, s = String(status || "").toUpperCase());
    }
    if ($$self.$$.dirty & /*s*/
    4) {
      $$invalidate(1, statusClass = s.includes("ONLINE") ? "ok" : s.includes("OFFLINE") ? "warn" : "bad");
    }
  };
  return [status, statusClass, s];
}
class StatusDot extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$7, create_fragment$7, safe_not_equal, { status: 0 });
  }
}
function get_each_context$4(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[5] = list[i];
  return child_ctx;
}
function create_if_block$5(ctx) {
  let statusdot;
  let current;
  statusdot = new StatusDot({ props: { status: (
    /*tab*/
    ctx[5].status
  ) } });
  return {
    c() {
      create_component(statusdot.$$.fragment);
    },
    m(target, anchor) {
      mount_component(statusdot, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const statusdot_changes = {};
      if (dirty & /*tabs*/
      1) statusdot_changes.status = /*tab*/
      ctx2[5].status;
      statusdot.$set(statusdot_changes);
    },
    i(local) {
      if (current) return;
      transition_in(statusdot.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(statusdot.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(statusdot, detaching);
    }
  };
}
function create_each_block$4(ctx) {
  let button;
  let t0;
  let t1_value = (
    /*tab*/
    ctx[5].label + ""
  );
  let t1;
  let t2;
  let button_class_value;
  let button_disabled_value;
  let current;
  let mounted;
  let dispose;
  let if_block = (
    /*tab*/
    ctx[5].status !== void 0 && create_if_block$5(ctx)
  );
  function click_handler() {
    return (
      /*click_handler*/
      ctx[3](
        /*tab*/
        ctx[5]
      )
    );
  }
  return {
    c() {
      button = element("button");
      if (if_block) if_block.c();
      t0 = space();
      t1 = text(t1_value);
      t2 = space();
      attr(button, "class", button_class_value = `tab ${/*activeTab*/
      ctx[1] === /*tab*/
      ctx[5].id ? "active" : ""}`);
      button.disabled = button_disabled_value = /*tab*/
      ctx[5].disabled;
      attr(button, "type", "button");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      if (if_block) if_block.m(button, null);
      append(button, t0);
      append(button, t1);
      append(button, t2);
      current = true;
      if (!mounted) {
        dispose = listen(button, "click", click_handler);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (
        /*tab*/
        ctx[5].status !== void 0
      ) {
        if (if_block) {
          if_block.p(ctx, dirty);
          if (dirty & /*tabs*/
          1) {
            transition_in(if_block, 1);
          }
        } else {
          if_block = create_if_block$5(ctx);
          if_block.c();
          transition_in(if_block, 1);
          if_block.m(button, t0);
        }
      } else if (if_block) {
        group_outros();
        transition_out(if_block, 1, 1, () => {
          if_block = null;
        });
        check_outros();
      }
      if ((!current || dirty & /*tabs*/
      1) && t1_value !== (t1_value = /*tab*/
      ctx[5].label + "")) set_data(t1, t1_value);
      if (!current || dirty & /*activeTab, tabs*/
      3 && button_class_value !== (button_class_value = `tab ${/*activeTab*/
      ctx[1] === /*tab*/
      ctx[5].id ? "active" : ""}`)) {
        attr(button, "class", button_class_value);
      }
      if (!current || dirty & /*tabs*/
      1 && button_disabled_value !== (button_disabled_value = /*tab*/
      ctx[5].disabled)) {
        button.disabled = button_disabled_value;
      }
    },
    i(local) {
      if (current) return;
      transition_in(if_block);
      current = true;
    },
    o(local) {
      transition_out(if_block);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(button);
      }
      if (if_block) if_block.d();
      mounted = false;
      dispose();
    }
  };
}
function create_fragment$6(ctx) {
  let nav;
  let current;
  let each_value = ensure_array_like(
    /*tabs*/
    ctx[0]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$4(get_each_context$4(ctx, each_value, i));
  }
  const out = (i) => transition_out(each_blocks[i], 1, 1, () => {
    each_blocks[i] = null;
  });
  return {
    c() {
      nav = element("nav");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(nav, "class", "tabs");
    },
    m(target, anchor) {
      insert(target, nav, anchor);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(nav, null);
        }
      }
      current = true;
    },
    p(ctx2, [dirty]) {
      if (dirty & /*activeTab, tabs, select, undefined*/
      7) {
        each_value = ensure_array_like(
          /*tabs*/
          ctx2[0]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$4(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
            transition_in(each_blocks[i], 1);
          } else {
            each_blocks[i] = create_each_block$4(child_ctx);
            each_blocks[i].c();
            transition_in(each_blocks[i], 1);
            each_blocks[i].m(nav, null);
          }
        }
        group_outros();
        for (i = each_value.length; i < each_blocks.length; i += 1) {
          out(i);
        }
        check_outros();
      }
    },
    i(local) {
      if (current) return;
      for (let i = 0; i < each_value.length; i += 1) {
        transition_in(each_blocks[i]);
      }
      current = true;
    },
    o(local) {
      each_blocks = each_blocks.filter(Boolean);
      for (let i = 0; i < each_blocks.length; i += 1) {
        transition_out(each_blocks[i]);
      }
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(nav);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function instance$6($$self, $$props, $$invalidate) {
  let { tabs = [] } = $$props;
  let { activeTab = "" } = $$props;
  const dispatch = createEventDispatcher();
  const select = (tab) => {
    if (!tab.disabled) dispatch("select", tab.id);
  };
  const click_handler = (tab) => select(tab);
  $$self.$$set = ($$props2) => {
    if ("tabs" in $$props2) $$invalidate(0, tabs = $$props2.tabs);
    if ("activeTab" in $$props2) $$invalidate(1, activeTab = $$props2.activeTab);
  };
  return [tabs, activeTab, select, click_handler];
}
class TabNav extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$6, create_fragment$6, safe_not_equal, { tabs: 0, activeTab: 1 });
  }
}
function get_each_context$3(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[5] = list[i];
  return child_ctx;
}
function create_each_block$3(ctx) {
  let button;
  let t0_value = (
    /*item*/
    ctx[5].label + ""
  );
  let t0;
  let t1;
  let button_class_value;
  let button_disabled_value;
  let mounted;
  let dispose;
  function click_handler() {
    return (
      /*click_handler*/
      ctx[3](
        /*item*/
        ctx[5]
      )
    );
  }
  return {
    c() {
      button = element("button");
      t0 = text(t0_value);
      t1 = space();
      attr(button, "class", button_class_value = `left-menu-item ${/*activeId*/
      ctx[1] === /*item*/
      ctx[5].id ? "active" : ""}`);
      button.disabled = button_disabled_value = /*item*/
      ctx[5].disabled;
      attr(button, "type", "button");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      append(button, t0);
      append(button, t1);
      if (!mounted) {
        dispose = listen(button, "click", click_handler);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty & /*items*/
      1 && t0_value !== (t0_value = /*item*/
      ctx[5].label + "")) set_data(t0, t0_value);
      if (dirty & /*activeId, items*/
      3 && button_class_value !== (button_class_value = `left-menu-item ${/*activeId*/
      ctx[1] === /*item*/
      ctx[5].id ? "active" : ""}`)) {
        attr(button, "class", button_class_value);
      }
      if (dirty & /*items*/
      1 && button_disabled_value !== (button_disabled_value = /*item*/
      ctx[5].disabled)) {
        button.disabled = button_disabled_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(button);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_fragment$5(ctx) {
  let aside;
  let div;
  let each_value = ensure_array_like(
    /*items*/
    ctx[0]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$3(get_each_context$3(ctx, each_value, i));
  }
  return {
    c() {
      aside = element("aside");
      div = element("div");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div, "class", "left-menu");
      attr(aside, "class", "left");
    },
    m(target, anchor) {
      insert(target, aside, anchor);
      append(aside, div);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div, null);
        }
      }
    },
    p(ctx2, [dirty]) {
      if (dirty & /*activeId, items, select*/
      7) {
        each_value = ensure_array_like(
          /*items*/
          ctx2[0]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$3(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block$3(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(aside);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function instance$5($$self, $$props, $$invalidate) {
  const dispatch = createEventDispatcher();
  let { items = [] } = $$props;
  let { activeId = "" } = $$props;
  const select = (item) => {
    if (!item.disabled) dispatch("select", item.id);
  };
  const click_handler = (item) => select(item);
  $$self.$$set = ($$props2) => {
    if ("items" in $$props2) $$invalidate(0, items = $$props2.items);
    if ("activeId" in $$props2) $$invalidate(1, activeId = $$props2.activeId);
  };
  return [items, activeId, select, click_handler];
}
class SideMenu extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$5, create_fragment$5, safe_not_equal, { items: 0, activeId: 1 });
  }
}
function get_each_context$2(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[14] = list[i];
  child_ctx[16] = i;
  return child_ctx;
}
function create_else_block$3(ctx) {
  let each_1_anchor;
  let current;
  let each_value = ensure_array_like(
    /*devices*/
    ctx[2]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$2(get_each_context$2(ctx, each_value, i));
  }
  const out = (i) => transition_out(each_blocks[i], 1, 1, () => {
    each_blocks[i] = null;
  });
  return {
    c() {
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      if (dirty & /*devices, fwString, selectedSerials, isRemovable, removing, toggleSelection*/
      828) {
        each_value = ensure_array_like(
          /*devices*/
          ctx2[2]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$2(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
            transition_in(each_blocks[i], 1);
          } else {
            each_blocks[i] = create_each_block$2(child_ctx);
            each_blocks[i].c();
            transition_in(each_blocks[i], 1);
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        group_outros();
        for (i = each_value.length; i < each_blocks.length; i += 1) {
          out(i);
        }
        check_outros();
      }
    },
    i(local) {
      if (current) return;
      for (let i = 0; i < each_value.length; i += 1) {
        transition_in(each_blocks[i]);
      }
      current = true;
    },
    o(local) {
      each_blocks = each_blocks.filter(Boolean);
      for (let i = 0; i < each_blocks.length; i += 1) {
        transition_out(each_blocks[i]);
      }
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_1$3(ctx) {
  let div;
  return {
    c() {
      div = element("div");
      div.textContent = "No keyers found.";
      attr(div, "class", "table-empty");
    },
    m(target, anchor) {
      insert(target, div, anchor);
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block$2(ctx) {
  let div5;
  let div0;
  let input;
  let input_checked_value;
  let input_disabled_value;
  let input_title_value;
  let t0;
  let div1;
  let t1_value = (
    /*d*/
    (ctx[14].name || "Device") + ""
  );
  let t1;
  let t2;
  let div2;
  let t3_value = (
    /*d*/
    (ctx[14].serial || "—") + ""
  );
  let t3;
  let t4;
  let div3;
  let t5_value = (
    /*fwString*/
    ctx[3](
      /*d*/
      ctx[14]
    ) + ""
  );
  let t5;
  let t6;
  let div4;
  let statusdot;
  let t7_value = (
    /*d*/
    (ctx[14].status || "—") + ""
  );
  let t7;
  let t8;
  let current;
  let mounted;
  let dispose;
  function change_handler() {
    return (
      /*change_handler*/
      ctx[12](
        /*d*/
        ctx[14]
      )
    );
  }
  statusdot = new StatusDot({ props: { status: (
    /*d*/
    ctx[14].status
  ) } });
  return {
    c() {
      div5 = element("div");
      div0 = element("div");
      input = element("input");
      t0 = space();
      div1 = element("div");
      t1 = text(t1_value);
      t2 = space();
      div2 = element("div");
      t3 = text(t3_value);
      t4 = space();
      div3 = element("div");
      t5 = text(t5_value);
      t6 = space();
      div4 = element("div");
      create_component(statusdot.$$.fragment);
      t7 = text(t7_value);
      t8 = space();
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*selectedSerials*/
      ctx[4].includes(
        /*d*/
        ctx[14].serial
      );
      input.disabled = input_disabled_value = !/*isRemovable*/
      ctx[8](
        /*d*/
        ctx[14]
      ) || /*removing*/
      ctx[5];
      attr(input, "title", input_title_value = /*isRemovable*/
      ctx[8](
        /*d*/
        ctx[14]
      ) ? "" : "Unplug the keyer to remove it");
      attr(div5, "class", `table-row ${/*i*/
      ctx[16] % 2 ? "alt" : ""}`);
    },
    m(target, anchor) {
      insert(target, div5, anchor);
      append(div5, div0);
      append(div0, input);
      append(div5, t0);
      append(div5, div1);
      append(div1, t1);
      append(div5, t2);
      append(div5, div2);
      append(div2, t3);
      append(div5, t4);
      append(div5, div3);
      append(div3, t5);
      append(div5, t6);
      append(div5, div4);
      mount_component(statusdot, div4, null);
      append(div4, t7);
      append(div5, t8);
      current = true;
      if (!mounted) {
        dispose = listen(input, "change", change_handler);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (!current || dirty & /*selectedSerials, devices*/
      20 && input_checked_value !== (input_checked_value = /*selectedSerials*/
      ctx[4].includes(
        /*d*/
        ctx[14].serial
      ))) {
        input.checked = input_checked_value;
      }
      if (!current || dirty & /*devices, removing*/
      36 && input_disabled_value !== (input_disabled_value = !/*isRemovable*/
      ctx[8](
        /*d*/
        ctx[14]
      ) || /*removing*/
      ctx[5])) {
        input.disabled = input_disabled_value;
      }
      if (!current || dirty & /*devices*/
      4 && input_title_value !== (input_title_value = /*isRemovable*/
      ctx[8](
        /*d*/
        ctx[14]
      ) ? "" : "Unplug the keyer to remove it")) {
        attr(input, "title", input_title_value);
      }
      if ((!current || dirty & /*devices*/
      4) && t1_value !== (t1_value = /*d*/
      (ctx[14].name || "Device") + "")) set_data(t1, t1_value);
      if ((!current || dirty & /*devices*/
      4) && t3_value !== (t3_value = /*d*/
      (ctx[14].serial || "—") + "")) set_data(t3, t3_value);
      if ((!current || dirty & /*fwString, devices*/
      12) && t5_value !== (t5_value = /*fwString*/
      ctx[3](
        /*d*/
        ctx[14]
      ) + "")) set_data(t5, t5_value);
      const statusdot_changes = {};
      if (dirty & /*devices*/
      4) statusdot_changes.status = /*d*/
      ctx[14].status;
      statusdot.$set(statusdot_changes);
      if ((!current || dirty & /*devices*/
      4) && t7_value !== (t7_value = /*d*/
      (ctx[14].status || "—") + "")) set_data(t7, t7_value);
    },
    i(local) {
      if (current) return;
      transition_in(statusdot.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(statusdot.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div5);
      }
      destroy_component(statusdot);
      mounted = false;
      dispose();
    }
  };
}
function create_if_block$4(ctx) {
  let div;
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(
        /*removeStatus*/
        ctx[6]
      );
      attr(div, "class", div_class_value = `inline-status ${/*removeStatusKind*/
      ctx[7] === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty & /*removeStatus*/
      64) set_data(
        t,
        /*removeStatus*/
        ctx2[6]
      );
      if (dirty & /*removeStatusKind*/
      128 && div_class_value !== (div_class_value = `inline-status ${/*removeStatusKind*/
      ctx2[7] === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_fragment$4(ctx) {
  var _a, _b, _c, _d, _e, _f, _g, _h, _i, _j;
  let section0;
  let div0;
  let t1;
  let div16;
  let div3;
  let div1;
  let t3;
  let div2;
  let t4_value = (
    /*runtime*/
    (((_b = (_a = ctx[0]) == null ? void 0 : _a.daemon) == null ? void 0 : _b.name) || "mhuxd") + ""
  );
  let t4;
  let t5;
  let t6_value = (
    /*runtime*/
    (((_d = (_c = ctx[0]) == null ? void 0 : _c.daemon) == null ? void 0 : _d.version) || "—") + ""
  );
  let t6;
  let t7;
  let div6;
  let div4;
  let t9;
  let div5;
  let t10_value = (
    /*runtime*/
    (((_e = ctx[0]) == null ? void 0 : _e.hostname) || "—") + ""
  );
  let t10;
  let t11;
  let div9;
  let div7;
  let t13;
  let div8;
  let t14_value = (
    /*runtime*/
    (((_g = (_f = ctx[0]) == null ? void 0 : _f.daemon) == null ? void 0 : _g.pid) ?? "—") + ""
  );
  let t14;
  let t15;
  let div12;
  let div10;
  let t17;
  let div11;
  let t18_value = (
    /*runtime*/
    (((_i = (_h = ctx[0]) == null ? void 0 : _h.daemon) == null ? void 0 : _i.logfile) || "—") + ""
  );
  let t18;
  let t19;
  let div15;
  let div13;
  let t21;
  let div14;
  let t22_value = (
    /*daemonCfg*/
    (((_j = ctx[1]) == null ? void 0 : _j.loglevel) || "—") + ""
  );
  let t22;
  let t23;
  let section1;
  let div17;
  let t25;
  let div24;
  let div23;
  let t34;
  let current_block_type_index;
  let if_block0;
  let t35;
  let div25;
  let button;
  let t36;
  let button_disabled_value;
  let t37;
  let current;
  let mounted;
  let dispose;
  const if_block_creators = [create_if_block_1$3, create_else_block$3];
  const if_blocks = [];
  function select_block_type(ctx2, dirty) {
    if (
      /*devices*/
      ctx2[2].length === 0
    ) return 0;
    return 1;
  }
  current_block_type_index = select_block_type(ctx);
  if_block0 = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx);
  let if_block1 = (
    /*removeStatus*/
    ctx[6] && create_if_block$4(ctx)
  );
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "Summary";
      t1 = space();
      div16 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Version:";
      t3 = space();
      div2 = element("div");
      t4 = text(t4_value);
      t5 = space();
      t6 = text(t6_value);
      t7 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Hostname:";
      t9 = space();
      div5 = element("div");
      t10 = text(t10_value);
      t11 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Process ID:";
      t13 = space();
      div8 = element("div");
      t14 = text(t14_value);
      t15 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "Log File:";
      t17 = space();
      div11 = element("div");
      t18 = text(t18_value);
      t19 = space();
      div15 = element("div");
      div13 = element("div");
      div13.textContent = "Loglevel:";
      t21 = space();
      div14 = element("div");
      t22 = text(t22_value);
      t23 = space();
      section1 = element("section");
      div17 = element("div");
      div17.textContent = "Keyer List";
      t25 = space();
      div24 = element("div");
      div23 = element("div");
      div23.innerHTML = `<div></div> <div>Name</div> <div>Serial</div> <div>Firmware</div> <div>Status</div>`;
      t34 = space();
      if_block0.c();
      t35 = space();
      div25 = element("div");
      button = element("button");
      t36 = text("Remove");
      t37 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "label");
      attr(div14, "class", "value");
      attr(div15, "class", "row");
      attr(div16, "class", "panel");
      attr(section0, "class", "section");
      attr(div17, "class", "section-title");
      attr(div23, "class", "table-header");
      attr(div24, "class", "panel table keyers-grid");
      attr(button, "class", "btn");
      button.disabled = button_disabled_value = !/*selectedSerials*/
      ctx[4].length || /*removing*/
      ctx[5];
      attr(div25, "class", "button-row");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div16);
      append(div16, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, t4);
      append(div2, t5);
      append(div2, t6);
      append(div16, t7);
      append(div16, div6);
      append(div6, div4);
      append(div6, t9);
      append(div6, div5);
      append(div5, t10);
      append(div16, t11);
      append(div16, div9);
      append(div9, div7);
      append(div9, t13);
      append(div9, div8);
      append(div8, t14);
      append(div16, t15);
      append(div16, div12);
      append(div12, div10);
      append(div12, t17);
      append(div12, div11);
      append(div11, t18);
      append(div16, t19);
      append(div16, div15);
      append(div15, div13);
      append(div15, t21);
      append(div15, div14);
      append(div14, t22);
      insert(target, t23, anchor);
      insert(target, section1, anchor);
      append(section1, div17);
      append(section1, t25);
      append(section1, div24);
      append(div24, div23);
      append(div24, t34);
      if_blocks[current_block_type_index].m(div24, null);
      append(section1, t35);
      append(section1, div25);
      append(div25, button);
      append(button, t36);
      append(section1, t37);
      if (if_block1) if_block1.m(section1, null);
      current = true;
      if (!mounted) {
        dispose = listen(
          button,
          "click",
          /*removeKeyers*/
          ctx[10]
        );
        mounted = true;
      }
    },
    p(ctx2, [dirty]) {
      var _a2, _b2, _c2, _d2, _e2, _f2, _g2, _h2, _i2, _j2;
      if ((!current || dirty & /*runtime*/
      1) && t4_value !== (t4_value = /*runtime*/
      (((_b2 = (_a2 = ctx2[0]) == null ? void 0 : _a2.daemon) == null ? void 0 : _b2.name) || "mhuxd") + "")) set_data(t4, t4_value);
      if ((!current || dirty & /*runtime*/
      1) && t6_value !== (t6_value = /*runtime*/
      (((_d2 = (_c2 = ctx2[0]) == null ? void 0 : _c2.daemon) == null ? void 0 : _d2.version) || "—") + "")) set_data(t6, t6_value);
      if ((!current || dirty & /*runtime*/
      1) && t10_value !== (t10_value = /*runtime*/
      (((_e2 = ctx2[0]) == null ? void 0 : _e2.hostname) || "—") + "")) set_data(t10, t10_value);
      if ((!current || dirty & /*runtime*/
      1) && t14_value !== (t14_value = /*runtime*/
      (((_g2 = (_f2 = ctx2[0]) == null ? void 0 : _f2.daemon) == null ? void 0 : _g2.pid) ?? "—") + "")) set_data(t14, t14_value);
      if ((!current || dirty & /*runtime*/
      1) && t18_value !== (t18_value = /*runtime*/
      (((_i2 = (_h2 = ctx2[0]) == null ? void 0 : _h2.daemon) == null ? void 0 : _i2.logfile) || "—") + "")) set_data(t18, t18_value);
      if ((!current || dirty & /*daemonCfg*/
      2) && t22_value !== (t22_value = /*daemonCfg*/
      (((_j2 = ctx2[1]) == null ? void 0 : _j2.loglevel) || "—") + "")) set_data(t22, t22_value);
      let previous_block_index = current_block_type_index;
      current_block_type_index = select_block_type(ctx2);
      if (current_block_type_index === previous_block_index) {
        if_blocks[current_block_type_index].p(ctx2, dirty);
      } else {
        group_outros();
        transition_out(if_blocks[previous_block_index], 1, 1, () => {
          if_blocks[previous_block_index] = null;
        });
        check_outros();
        if_block0 = if_blocks[current_block_type_index];
        if (!if_block0) {
          if_block0 = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx2);
          if_block0.c();
        } else {
          if_block0.p(ctx2, dirty);
        }
        transition_in(if_block0, 1);
        if_block0.m(div24, null);
      }
      if (!current || dirty & /*selectedSerials, removing*/
      48 && button_disabled_value !== (button_disabled_value = !/*selectedSerials*/
      ctx2[4].length || /*removing*/
      ctx2[5])) {
        button.disabled = button_disabled_value;
      }
      if (
        /*removeStatus*/
        ctx2[6]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block$4(ctx2);
          if_block1.c();
          if_block1.m(section1, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    i(local) {
      if (current) return;
      transition_in(if_block0);
      current = true;
    },
    o(local) {
      transition_out(if_block0);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t23);
        detach(section1);
      }
      if_blocks[current_block_type_index].d();
      if (if_block1) if_block1.d();
      mounted = false;
      dispose();
    }
  };
}
function instance$4($$self, $$props, $$invalidate) {
  let { runtime = null } = $$props;
  let { daemonCfg = null } = $$props;
  let { devices = [] } = $$props;
  let { fwString = () => "" } = $$props;
  let { reloadData = async () => {
  } } = $$props;
  let selectedSerials = [];
  let removing = false;
  let removeStatus = "";
  let removeStatusKind = "success";
  let removeStatusTimer;
  const isRemovable = (d) => d.status === "DISCONNECTED";
  const toggleSelection = (serial) => {
    $$invalidate(4, selectedSerials = selectedSerials.includes(serial) ? selectedSerials.filter((s) => s !== serial) : [...selectedSerials, serial]);
  };
  const removeKeyers = async () => {
    if (!selectedSerials.length) return;
    const names = selectedSerials.join(", ");
    if (!confirm(`Remove ${names}?

All settings of the keyer(s) and their ports will be discarded. A removed keyer shows up again with default settings when it gets plugged in.`)) return;
    $$invalidate(5, removing = true);
    $$invalidate(6, removeStatus = "");
    $$invalidate(7, removeStatusKind = "success");
    try {
      await Promise.all(selectedSerials.map(async (serial) => {
        const res = await fetch(`/api/v1/config/devices/${encodeURIComponent(serial)}`, {
          method: "DELETE",
          headers: { Accept: "application/json" }
        });
        if (!res.ok) {
          const body = await res.json().catch(() => ({}));
          throw new Error(`${serial}: ${body.error || res.status}`);
        }
      }));
      $$invalidate(4, selectedSerials = []);
      $$invalidate(6, removeStatus = "Keyer(s) removed.");
    } catch (err) {
      $$invalidate(6, removeStatus = (err == null ? void 0 : err.message) || "Failed to remove keyer(s).");
      $$invalidate(7, removeStatusKind = "error");
    } finally {
      $$invalidate(5, removing = false);
      await reloadData().catch(() => {
      });
      clearTimeout(removeStatusTimer);
      removeStatusTimer = setTimeout(
        () => {
          $$invalidate(6, removeStatus = "");
        },
        5e3
      );
    }
  };
  const change_handler = (d) => toggleSelection(d.serial);
  $$self.$$set = ($$props2) => {
    if ("runtime" in $$props2) $$invalidate(0, runtime = $$props2.runtime);
    if ("daemonCfg" in $$props2) $$invalidate(1, daemonCfg = $$props2.daemonCfg);
    if ("devices" in $$props2) $$invalidate(2, devices = $$props2.devices);
    if ("fwString" in $$props2) $$invalidate(3, fwString = $$props2.fwString);
    if ("reloadData" in $$props2) $$invalidate(11, reloadData = $$props2.reloadData);
  };
  $$self.$$.update = () => {
    if ($$self.$$.dirty & /*selectedSerials, devices*/
    20) {
      $$invalidate(4, selectedSerials = selectedSerials.filter((serial) => devices.some((d) => d.serial === serial && isRemovable(d))));
    }
  };
  return [
    runtime,
    daemonCfg,
    devices,
    fwString,
    selectedSerials,
    removing,
    removeStatus,
    removeStatusKind,
    isRemovable,
    toggleSelection,
    removeKeyers,
    reloadData,
    change_handler
  ];
}
class HomeSummary extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$4, create_fragment$4, safe_not_equal, {
      runtime: 0,
      daemonCfg: 1,
      devices: 2,
      fwString: 3,
      reloadData: 11
    });
  }
}
function get_each_context$1(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[46] = list[i];
  return child_ctx;
}
function get_each_context_1$1(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[49] = list[i];
  return child_ctx;
}
function get_each_context_2$1(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[52] = list[i];
  return child_ctx;
}
function get_each_context_3$1(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[55] = list[i];
  child_ctx[57] = i;
  return child_ctx;
}
function create_else_block_3$1(ctx) {
  let each_1_anchor;
  let each_value_3 = ensure_array_like(
    /*connectors*/
    ctx[0]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_3.length; i += 1) {
    each_blocks[i] = create_each_block_3$1(get_each_context_3$1(ctx, each_value_3, i));
  }
  return {
    c() {
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*displayPortChannel, connectors, deviceNameForSerial, checkMark, selectedConnectorIds, toggleConnectorSelection*/
      217601) {
        each_value_3 = ensure_array_like(
          /*connectors*/
          ctx2[0]
        );
        let i;
        for (i = 0; i < each_value_3.length; i += 1) {
          const child_ctx = get_each_context_3$1(ctx2, each_value_3, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_3$1(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_3.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_6$1(ctx) {
  let div;
  return {
    c() {
      div = element("div");
      div.textContent = "No ports configured.";
      attr(div, "class", "table-empty");
    },
    m(target, anchor) {
      insert(target, div, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block_3$1(ctx) {
  let div11;
  let div0;
  let input;
  let input_checked_value;
  let input_disabled_value;
  let t0;
  let div1;
  let t1_value = (
    /*c*/
    (ctx[55].id ?? "—") + ""
  );
  let t1;
  let t2;
  let div2;
  let t3_value = (
    /*c*/
    (ctx[55].type || "—") + ""
  );
  let t3;
  let t4;
  let div3;
  let t5_value = (
    /*c*/
    (ctx[55].devname || "—") + ""
  );
  let t5;
  let t6;
  let div4;
  let t7_value = (
    /*c*/
    (ctx[55].status || "—") + ""
  );
  let t7;
  let div4_class_value;
  let t8;
  let div5;
  let t9_value = (
    /*c*/
    (ctx[55].type === "TCP" ? (
      /*checkMark*/
      ctx[16](
        /*c*/
        ctx[55].remote_access
      )
    ) : "—") + ""
  );
  let t9;
  let t10;
  let div6;
  let t11_value = (
    /*c*/
    (ctx[55].type === "VSP" ? (
      /*checkMark*/
      ctx[16](
        /*c*/
        ctx[55].ptt_rts
      )
    ) : "—") + ""
  );
  let t11;
  let t12;
  let div7;
  let t13_value = (
    /*c*/
    (ctx[55].type === "VSP" ? (
      /*checkMark*/
      ctx[16](
        /*c*/
        ctx[55].ptt_dtr
      )
    ) : "—") + ""
  );
  let t13;
  let t14;
  let div8;
  let t15_value = (
    /*deviceNameForSerial*/
    ctx[12](
      /*c*/
      ctx[55].serial
    ) + ""
  );
  let t15;
  let t16;
  let div9;
  let t17_value = (
    /*c*/
    (ctx[55].serial || "—") + ""
  );
  let t17;
  let t18;
  let div10;
  let t19_value = (
    /*displayPortChannel*/
    ctx[14](
      /*c*/
      ctx[55].channel
    ) + ""
  );
  let t19;
  let t20;
  let mounted;
  let dispose;
  function change_handler() {
    return (
      /*change_handler*/
      ctx[24](
        /*c*/
        ctx[55]
      )
    );
  }
  return {
    c() {
      div11 = element("div");
      div0 = element("div");
      input = element("input");
      t0 = space();
      div1 = element("div");
      t1 = text(t1_value);
      t2 = space();
      div2 = element("div");
      t3 = text(t3_value);
      t4 = space();
      div3 = element("div");
      t5 = text(t5_value);
      t6 = space();
      div4 = element("div");
      t7 = text(t7_value);
      t8 = space();
      div5 = element("div");
      t9 = text(t9_value);
      t10 = space();
      div6 = element("div");
      t11 = text(t11_value);
      t12 = space();
      div7 = element("div");
      t13 = text(t13_value);
      t14 = space();
      div8 = element("div");
      t15 = text(t15_value);
      t16 = space();
      div9 = element("div");
      t17 = text(t17_value);
      t18 = space();
      div10 = element("div");
      t19 = text(t19_value);
      t20 = space();
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*selectedConnectorIds*/
      ctx[9].includes(
        /*c*/
        ctx[55].id
      );
      input.disabled = input_disabled_value = /*c*/
      ctx[55].id == null;
      attr(div4, "class", div4_class_value = /*c*/
      ctx[55].status === "failed" ? "text-error" : "");
      attr(div11, "class", null_to_empty(`table-row ${/*i*/
      ctx[57] % 2 ? "alt" : ""}`) + " svelte-udangm");
    },
    m(target, anchor) {
      insert(target, div11, anchor);
      append(div11, div0);
      append(div0, input);
      append(div11, t0);
      append(div11, div1);
      append(div1, t1);
      append(div11, t2);
      append(div11, div2);
      append(div2, t3);
      append(div11, t4);
      append(div11, div3);
      append(div3, t5);
      append(div11, t6);
      append(div11, div4);
      append(div4, t7);
      append(div11, t8);
      append(div11, div5);
      append(div5, t9);
      append(div11, t10);
      append(div11, div6);
      append(div6, t11);
      append(div11, t12);
      append(div11, div7);
      append(div7, t13);
      append(div11, t14);
      append(div11, div8);
      append(div8, t15);
      append(div11, t16);
      append(div11, div9);
      append(div9, t17);
      append(div11, t18);
      append(div11, div10);
      append(div10, t19);
      append(div11, t20);
      if (!mounted) {
        dispose = listen(input, "change", change_handler);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*selectedConnectorIds, connectors*/
      513 && input_checked_value !== (input_checked_value = /*selectedConnectorIds*/
      ctx[9].includes(
        /*c*/
        ctx[55].id
      ))) {
        input.checked = input_checked_value;
      }
      if (dirty[0] & /*connectors*/
      1 && input_disabled_value !== (input_disabled_value = /*c*/
      ctx[55].id == null)) {
        input.disabled = input_disabled_value;
      }
      if (dirty[0] & /*connectors*/
      1 && t1_value !== (t1_value = /*c*/
      (ctx[55].id ?? "—") + "")) set_data(t1, t1_value);
      if (dirty[0] & /*connectors*/
      1 && t3_value !== (t3_value = /*c*/
      (ctx[55].type || "—") + "")) set_data(t3, t3_value);
      if (dirty[0] & /*connectors*/
      1 && t5_value !== (t5_value = /*c*/
      (ctx[55].devname || "—") + "")) set_data(t5, t5_value);
      if (dirty[0] & /*connectors*/
      1 && t7_value !== (t7_value = /*c*/
      (ctx[55].status || "—") + "")) set_data(t7, t7_value);
      if (dirty[0] & /*connectors*/
      1 && div4_class_value !== (div4_class_value = /*c*/
      ctx[55].status === "failed" ? "text-error" : "")) {
        attr(div4, "class", div4_class_value);
      }
      if (dirty[0] & /*connectors*/
      1 && t9_value !== (t9_value = /*c*/
      (ctx[55].type === "TCP" ? (
        /*checkMark*/
        ctx[16](
          /*c*/
          ctx[55].remote_access
        )
      ) : "—") + "")) set_data(t9, t9_value);
      if (dirty[0] & /*connectors*/
      1 && t11_value !== (t11_value = /*c*/
      (ctx[55].type === "VSP" ? (
        /*checkMark*/
        ctx[16](
          /*c*/
          ctx[55].ptt_rts
        )
      ) : "—") + "")) set_data(t11, t11_value);
      if (dirty[0] & /*connectors*/
      1 && t13_value !== (t13_value = /*c*/
      (ctx[55].type === "VSP" ? (
        /*checkMark*/
        ctx[16](
          /*c*/
          ctx[55].ptt_dtr
        )
      ) : "—") + "")) set_data(t13, t13_value);
      if (dirty[0] & /*connectors*/
      1 && t15_value !== (t15_value = /*deviceNameForSerial*/
      ctx[12](
        /*c*/
        ctx[55].serial
      ) + "")) set_data(t15, t15_value);
      if (dirty[0] & /*connectors*/
      1 && t17_value !== (t17_value = /*c*/
      (ctx[55].serial || "—") + "")) set_data(t17, t17_value);
      if (dirty[0] & /*connectors*/
      1 && t19_value !== (t19_value = /*displayPortChannel*/
      ctx[14](
        /*c*/
        ctx[55].channel
      ) + "")) set_data(t19, t19_value);
    },
    d(detaching) {
      if (detaching) {
        detach(div11);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_5$2(ctx) {
  let div;
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(
        /*portRemoveStatus*/
        ctx[6]
      );
      attr(div, "class", div_class_value = null_to_empty(`inline-status ${/*portRemoveStatusKind*/
      ctx[7] === "error" ? "error" : ""}`) + " svelte-udangm");
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portRemoveStatus*/
      64) set_data(
        t,
        /*portRemoveStatus*/
        ctx2[6]
      );
      if (dirty[0] & /*portRemoveStatusKind*/
      128 && div_class_value !== (div_class_value = null_to_empty(`inline-status ${/*portRemoveStatusKind*/
      ctx2[7] === "error" ? "error" : ""}`) + " svelte-udangm")) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block$3(ctx) {
  let div17;
  let div16;
  let div0;
  let t0;
  let button0;
  let t2;
  let div15;
  let div4;
  let div1;
  let t4;
  let div3;
  let div2;
  let t5;
  let div7;
  let div5;
  let t7;
  let div6;
  let select0;
  let select0_value_value;
  let t8;
  let div10;
  let div8;
  let t10;
  let div9;
  let select1;
  let show_if;
  let select1_disabled_value;
  let t11;
  let t12;
  let div13;
  let div11;
  let t14;
  let div12;
  let input;
  let t15;
  let div14;
  let button1;
  let t16_value = (
    /*portSaving*/
    ctx[8] ? "Saving…" : "Create"
  );
  let t16;
  let button1_disabled_value;
  let t17;
  let button2;
  let t18;
  let t19;
  let mounted;
  let dispose;
  let each_value_2 = ensure_array_like(
    /*portTypeOptions*/
    ctx[11]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_2.length; i += 1) {
    each_blocks[i] = create_each_block_2$1(get_each_context_2$1(ctx, each_value_2, i));
  }
  function select_block_type_1(ctx2, dirty) {
    if (
      /*keyers*/
      ctx2[1].length === 0
    ) return create_if_block_4$2;
    return create_else_block_2$1;
  }
  let current_block_type = select_block_type_1(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_2(ctx2, dirty) {
    if (dirty[0] & /*portForm*/
    4) show_if = null;
    if (show_if == null) show_if = !!/*channelOptionsForSerial*/
    (ctx2[13](
      /*portForm*/
      ctx2[2].serial
    ).length === 0);
    if (show_if) return create_if_block_3$2;
    return create_else_block_1$1;
  }
  let current_block_type_1 = select_block_type_2(ctx, [-1, -1]);
  let if_block1 = current_block_type_1(ctx);
  function select_block_type_3(ctx2, dirty) {
    if (
      /*portForm*/
      ctx2[2].type === "VSP"
    ) return create_if_block_2$2;
    return create_else_block$2;
  }
  let current_block_type_2 = select_block_type_3(ctx);
  let if_block2 = current_block_type_2(ctx);
  let if_block3 = (
    /*portStatus*/
    ctx[4] && create_if_block_1$2(ctx)
  );
  return {
    c() {
      div17 = element("div");
      div16 = element("div");
      div0 = element("div");
      t0 = text("Add Port\n    ");
      button0 = element("button");
      button0.textContent = "✕";
      t2 = space();
      div15 = element("div");
      div4 = element("div");
      div1 = element("div");
      div1.textContent = "Port Type:";
      t4 = space();
      div3 = element("div");
      div2 = element("div");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t5 = space();
      div7 = element("div");
      div5 = element("div");
      div5.textContent = "Destination Keyer:";
      t7 = space();
      div6 = element("div");
      select0 = element("select");
      if_block0.c();
      t8 = space();
      div10 = element("div");
      div8 = element("div");
      div8.textContent = "Destination Channel:";
      t10 = space();
      div9 = element("div");
      select1 = element("select");
      if_block1.c();
      t11 = space();
      if_block2.c();
      t12 = space();
      div13 = element("div");
      div11 = element("div");
      div11.textContent = "Max Connections:";
      t14 = space();
      div12 = element("div");
      input = element("input");
      t15 = space();
      div14 = element("div");
      button1 = element("button");
      t16 = text(t16_value);
      t17 = space();
      button2 = element("button");
      t18 = text("Cancel");
      t19 = space();
      if (if_block3) if_block3.c();
      attr(button0, "class", "overlay-close-btn svelte-udangm");
      attr(div0, "class", "section-title add-port-modal-title svelte-udangm");
      attr(div1, "class", "label");
      attr(div2, "class", "inline-list");
      attr(div3, "class", "value");
      attr(div4, "class", "row");
      attr(div5, "class", "label");
      attr(select0, "class", "select");
      attr(div6, "class", "value");
      attr(div7, "class", "row");
      attr(div8, "class", "label");
      attr(select1, "class", "select");
      select1.disabled = select1_disabled_value = /*channelOptionsForSerial*/
      ctx[13](
        /*portForm*/
        ctx[2].serial
      ).length === 0;
      if (
        /*portForm*/
        ctx[2].channel === void 0
      ) add_render_callback(() => (
        /*select1_change_handler*/
        ctx[30].call(select1)
      ));
      attr(div9, "class", "value");
      attr(div10, "class", "row");
      attr(div11, "class", "label");
      attr(input, "class", "input");
      attr(input, "type", "number");
      attr(input, "min", "1");
      attr(div12, "class", "value");
      attr(div13, "class", "row");
      attr(button1, "class", "btn");
      button1.disabled = button1_disabled_value = /*portSaving*/
      ctx[8] || /*keyers*/
      ctx[1].length === 0;
      attr(button2, "class", "btn");
      button2.disabled = /*portSaving*/
      ctx[8];
      attr(div14, "class", "button-row svelte-udangm");
      attr(div15, "class", "panel");
      attr(div16, "class", "add-port-modal svelte-udangm");
      attr(div17, "class", "add-port-backdrop svelte-udangm");
    },
    m(target, anchor) {
      insert(target, div17, anchor);
      append(div17, div16);
      append(div16, div0);
      append(div0, t0);
      append(div0, button0);
      append(div16, t2);
      append(div16, div15);
      append(div15, div4);
      append(div4, div1);
      append(div4, t4);
      append(div4, div3);
      append(div3, div2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div2, null);
        }
      }
      append(div15, t5);
      append(div15, div7);
      append(div7, div5);
      append(div7, t7);
      append(div7, div6);
      append(div6, select0);
      if_block0.m(select0, null);
      select_option(
        select0,
        /*portForm*/
        ctx[2].serial
      );
      append(div15, t8);
      append(div15, div10);
      append(div10, div8);
      append(div10, t10);
      append(div10, div9);
      append(div9, select1);
      if_block1.m(select1, null);
      select_option(
        select1,
        /*portForm*/
        ctx[2].channel,
        true
      );
      append(div15, t11);
      if_block2.m(div15, null);
      append(div15, t12);
      append(div15, div13);
      append(div13, div11);
      append(div13, t14);
      append(div13, div12);
      append(div12, input);
      set_input_value(
        input,
        /*portForm*/
        ctx[2].maxcon
      );
      append(div15, t15);
      append(div15, div14);
      append(div14, button1);
      append(button1, t16);
      append(div14, t17);
      append(div14, button2);
      append(button2, t18);
      append(div15, t19);
      if (if_block3) if_block3.m(div15, null);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler*/
            ctx[25]
          ),
          listen(
            select0,
            "change",
            /*change_handler_2*/
            ctx[29]
          ),
          listen(
            select1,
            "change",
            /*select1_change_handler*/
            ctx[30]
          ),
          listen(
            input,
            "input",
            /*input_input_handler*/
            ctx[38]
          ),
          listen(
            button1,
            "click",
            /*applyPort*/
            ctx[18]
          ),
          listen(
            button2,
            "click",
            /*click_handler_1*/
            ctx[39]
          ),
          listen(div17, "click", self(
            /*click_handler_2*/
            ctx[40]
          ))
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portTypeOptions, portForm, devnameTouched*/
      2060) {
        each_value_2 = ensure_array_like(
          /*portTypeOptions*/
          ctx2[11]
        );
        let i;
        for (i = 0; i < each_value_2.length; i += 1) {
          const child_ctx = get_each_context_2$1(ctx2, each_value_2, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_2$1(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_2.length;
      }
      if (current_block_type === (current_block_type = select_block_type_1(ctx2)) && if_block0) {
        if_block0.p(ctx2, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx2);
        if (if_block0) {
          if_block0.c();
          if_block0.m(select0, null);
        }
      }
      if (dirty[0] & /*portForm, keyers*/
      6 && select0_value_value !== (select0_value_value = /*portForm*/
      ctx2[2].serial)) {
        select_option(
          select0,
          /*portForm*/
          ctx2[2].serial
        );
      }
      if (current_block_type_1 === (current_block_type_1 = select_block_type_2(ctx2, dirty)) && if_block1) {
        if_block1.p(ctx2, dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(ctx2);
        if (if_block1) {
          if_block1.c();
          if_block1.m(select1, null);
        }
      }
      if (dirty[0] & /*portForm, keyers*/
      6 && select1_disabled_value !== (select1_disabled_value = /*channelOptionsForSerial*/
      ctx2[13](
        /*portForm*/
        ctx2[2].serial
      ).length === 0)) {
        select1.disabled = select1_disabled_value;
      }
      if (dirty[0] & /*portForm, keyers*/
      6) {
        select_option(
          select1,
          /*portForm*/
          ctx2[2].channel
        );
      }
      if (current_block_type_2 === (current_block_type_2 = select_block_type_3(ctx2)) && if_block2) {
        if_block2.p(ctx2, dirty);
      } else {
        if_block2.d(1);
        if_block2 = current_block_type_2(ctx2);
        if (if_block2) {
          if_block2.c();
          if_block2.m(div15, t12);
        }
      }
      if (dirty[0] & /*portForm, keyers*/
      6 && to_number(input.value) !== /*portForm*/
      ctx2[2].maxcon) {
        set_input_value(
          input,
          /*portForm*/
          ctx2[2].maxcon
        );
      }
      if (dirty[0] & /*portSaving*/
      256 && t16_value !== (t16_value = /*portSaving*/
      ctx2[8] ? "Saving…" : "Create")) set_data(t16, t16_value);
      if (dirty[0] & /*portSaving, keyers*/
      258 && button1_disabled_value !== (button1_disabled_value = /*portSaving*/
      ctx2[8] || /*keyers*/
      ctx2[1].length === 0)) {
        button1.disabled = button1_disabled_value;
      }
      if (dirty[0] & /*portSaving*/
      256) {
        button2.disabled = /*portSaving*/
        ctx2[8];
      }
      if (
        /*portStatus*/
        ctx2[4]
      ) {
        if (if_block3) {
          if_block3.p(ctx2, dirty);
        } else {
          if_block3 = create_if_block_1$2(ctx2);
          if_block3.c();
          if_block3.m(div15, null);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div17);
      }
      destroy_each(each_blocks, detaching);
      if_block0.d();
      if_block1.d();
      if_block2.d();
      if (if_block3) if_block3.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_2$1(ctx) {
  let label;
  let input;
  let t0;
  let t1_value = (
    /*opt*/
    ctx[52] === "VSP" ? "VSP Virtual Serial Port" : "TCP Network Port"
  );
  let t1;
  let t2;
  let binding_group;
  let mounted;
  let dispose;
  binding_group = init_binding_group(
    /*$$binding_groups*/
    ctx[27][0]
  );
  return {
    c() {
      label = element("label");
      input = element("input");
      t0 = space();
      t1 = text(t1_value);
      t2 = space();
      attr(input, "type", "radio");
      input.__value = /*opt*/
      ctx[52];
      set_input_value(input, input.__value);
      attr(label, "class", "radio-option");
      binding_group.p(input);
    },
    m(target, anchor) {
      insert(target, label, anchor);
      append(label, input);
      input.checked = input.__value === /*portForm*/
      ctx[2].type;
      append(label, t0);
      append(label, t1);
      append(label, t2);
      if (!mounted) {
        dispose = [
          listen(
            input,
            "change",
            /*input_change_handler*/
            ctx[26]
          ),
          listen(
            input,
            "change",
            /*change_handler_1*/
            ctx[28]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portForm, keyers*/
      6) {
        input.checked = input.__value === /*portForm*/
        ctx2[2].type;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(label);
      }
      binding_group.r();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_else_block_2$1(ctx) {
  let each_1_anchor;
  let each_value_1 = ensure_array_like(
    /*keyers*/
    ctx[1]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_1.length; i += 1) {
    each_blocks[i] = create_each_block_1$1(get_each_context_1$1(ctx, each_value_1, i));
  }
  return {
    c() {
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyers*/
      2) {
        each_value_1 = ensure_array_like(
          /*keyers*/
          ctx2[1]
        );
        let i;
        for (i = 0; i < each_value_1.length; i += 1) {
          const child_ctx = get_each_context_1$1(ctx2, each_value_1, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_1$1(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_1.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_4$2(ctx) {
  let option;
  return {
    c() {
      option = element("option");
      option.textContent = "No keyers available";
      option.__value = "";
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_1$1(ctx) {
  let option;
  let t0_value = (
    /*k*/
    ctx[49].name + ""
  );
  let t0;
  let t1;
  let t2_value = (
    /*k*/
    ctx[49].serial + ""
  );
  let t2;
  let t3;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t0 = text(t0_value);
      t1 = text(" (");
      t2 = text(t2_value);
      t3 = text(")");
      option.__value = option_value_value = /*k*/
      ctx[49].serial;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t0);
      append(option, t1);
      append(option, t2);
      append(option, t3);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyers*/
      2 && t0_value !== (t0_value = /*k*/
      ctx2[49].name + "")) set_data(t0, t0_value);
      if (dirty[0] & /*keyers*/
      2 && t2_value !== (t2_value = /*k*/
      ctx2[49].serial + "")) set_data(t2, t2_value);
      if (dirty[0] & /*keyers*/
      2 && option_value_value !== (option_value_value = /*k*/
      ctx2[49].serial)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_else_block_1$1(ctx) {
  let each_1_anchor;
  let each_value = ensure_array_like(
    /*channelOptionsForSerial*/
    ctx[13](
      /*portForm*/
      ctx[2].serial
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$1(get_each_context$1(ctx, each_value, i));
  }
  return {
    c() {
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*channelOptionsForSerial, portForm*/
      8196) {
        each_value = ensure_array_like(
          /*channelOptionsForSerial*/
          ctx2[13](
            /*portForm*/
            ctx2[2].serial
          )
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$1(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block$1(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_3$2(ctx) {
  let option;
  return {
    c() {
      option = element("option");
      option.textContent = "No channels";
      option.__value = "";
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block$1(ctx) {
  let option;
  let t_value = (
    /*ch*/
    ctx[46] + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*ch*/
      ctx[46];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portForm*/
      4 && t_value !== (t_value = /*ch*/
      ctx2[46] + "")) set_data(t, t_value);
      if (dirty[0] & /*portForm, keyers*/
      6 && option_value_value !== (option_value_value = /*ch*/
      ctx2[46])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_else_block$2(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input0;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input1;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Port Number:";
      t1 = space();
      div1 = element("div");
      input0 = element("input");
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "Remote Accessible:";
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      attr(div0, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      attr(input0, "min", "1");
      attr(input0, "step", "1");
      attr(input0, "inputmode", "numeric");
      attr(input0, "placeholder", "9001");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "type", "checkbox");
      attr(div4, "class", "value");
      attr(div5, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input0);
      set_input_value(
        input0,
        /*portForm*/
        ctx[2].devname
      );
      insert(target, t2, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input1);
      input1.checked = /*portForm*/
      ctx[2].remote_access;
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler_1*/
            ctx[35]
          ),
          listen(
            input0,
            "input",
            /*input_handler_1*/
            ctx[36]
          ),
          listen(
            input1,
            "change",
            /*input1_change_handler_1*/
            ctx[37]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portForm, keyers*/
      6 && to_number(input0.value) !== /*portForm*/
      ctx2[2].devname) {
        set_input_value(
          input0,
          /*portForm*/
          ctx2[2].devname
        );
      }
      if (dirty[0] & /*portForm, keyers*/
      6) {
        input1.checked = /*portForm*/
        ctx2[2].remote_access;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t2);
        detach(div5);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_2$2(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let t2;
  let input0;
  let t3;
  let div5;
  let div3;
  let t5;
  let div4;
  let input1;
  let t6;
  let div8;
  let div6;
  let t8;
  let div7;
  let input2;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Device Path:";
      t1 = space();
      div1 = element("div");
      t2 = text("/dev/mhuxd/\n          ");
      input0 = element("input");
      t3 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "PTT via RTS:";
      t5 = space();
      div4 = element("div");
      input1 = element("input");
      t6 = space();
      div8 = element("div");
      div6 = element("div");
      div6.textContent = "PTT via DTR:";
      t8 = space();
      div7 = element("div");
      input2 = element("input");
      attr(div0, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "maxlength", VSP_DEVNAME_MAX);
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "type", "checkbox");
      attr(div4, "class", "value");
      attr(div5, "class", "row");
      attr(div6, "class", "label");
      attr(input2, "type", "checkbox");
      attr(div7, "class", "value");
      attr(div8, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, t2);
      append(div1, input0);
      set_input_value(
        input0,
        /*portForm*/
        ctx[2].devname
      );
      insert(target, t3, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t5);
      append(div5, div4);
      append(div4, input1);
      input1.checked = /*portForm*/
      ctx[2].ptt_rts;
      insert(target, t6, anchor);
      insert(target, div8, anchor);
      append(div8, div6);
      append(div8, t8);
      append(div8, div7);
      append(div7, input2);
      input2.checked = /*portForm*/
      ctx[2].ptt_dtr;
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler*/
            ctx[31]
          ),
          listen(
            input0,
            "input",
            /*input_handler*/
            ctx[32]
          ),
          listen(
            input1,
            "change",
            /*input1_change_handler*/
            ctx[33]
          ),
          listen(
            input2,
            "change",
            /*input2_change_handler*/
            ctx[34]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portForm, keyers*/
      6 && input0.value !== /*portForm*/
      ctx2[2].devname) {
        set_input_value(
          input0,
          /*portForm*/
          ctx2[2].devname
        );
      }
      if (dirty[0] & /*portForm, keyers*/
      6) {
        input1.checked = /*portForm*/
        ctx2[2].ptt_rts;
      }
      if (dirty[0] & /*portForm, keyers*/
      6) {
        input2.checked = /*portForm*/
        ctx2[2].ptt_dtr;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t3);
        detach(div5);
        detach(t6);
        detach(div8);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_1$2(ctx) {
  let div;
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(
        /*portStatus*/
        ctx[4]
      );
      attr(div, "class", div_class_value = null_to_empty(`inline-status ${/*portStatusKind*/
      ctx[5] === "error" ? "error" : ""}`) + " svelte-udangm");
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*portStatus*/
      16) set_data(
        t,
        /*portStatus*/
        ctx2[4]
      );
      if (dirty[0] & /*portStatusKind*/
      32 && div_class_value !== (div_class_value = null_to_empty(`inline-status ${/*portStatusKind*/
      ctx2[5] === "error" ? "error" : ""}`) + " svelte-udangm")) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_fragment$3(ctx) {
  let section;
  let div0;
  let t1;
  let div13;
  let div12;
  let t22;
  let t23;
  let div14;
  let button0;
  let t25;
  let button1;
  let t26;
  let button1_disabled_value;
  let t27;
  let t28;
  let if_block2_anchor;
  let mounted;
  let dispose;
  function select_block_type(ctx2, dirty) {
    if (
      /*connectors*/
      ctx2[0].length === 0
    ) return create_if_block_6$1;
    return create_else_block_3$1;
  }
  let current_block_type = select_block_type(ctx);
  let if_block0 = current_block_type(ctx);
  let if_block1 = (
    /*portRemoveStatus*/
    ctx[6] && create_if_block_5$2(ctx)
  );
  let if_block2 = (
    /*showAddPortOverlay*/
    ctx[10] && create_if_block$3(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Port List";
      t1 = space();
      div13 = element("div");
      div12 = element("div");
      div12.innerHTML = `<div></div> <div>ID</div> <div>Type</div> <div>Port / Device</div> <div>Status</div> <div>Remote Access</div> <div>RTS-PTT</div> <div>DTR-PTT</div> <div>Destination Name</div> <div>Destination Serial</div> <div>Channel</div>`;
      t22 = space();
      if_block0.c();
      t23 = space();
      div14 = element("div");
      button0 = element("button");
      button0.textContent = "Add";
      t25 = space();
      button1 = element("button");
      t26 = text("Remove");
      t27 = space();
      if (if_block1) if_block1.c();
      t28 = space();
      if (if_block2) if_block2.c();
      if_block2_anchor = empty();
      attr(div0, "class", "section-title");
      attr(div12, "class", "table-header");
      attr(div13, "class", "panel table ports-grid");
      attr(button0, "class", "btn");
      attr(button1, "class", "btn");
      button1.disabled = button1_disabled_value = !/*selectedConnectorIds*/
      ctx[9].length || /*portSaving*/
      ctx[8];
      attr(div14, "class", "button-row svelte-udangm");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div13);
      append(div13, div12);
      append(div13, t22);
      if_block0.m(div13, null);
      append(section, t23);
      append(section, div14);
      append(div14, button0);
      append(div14, t25);
      append(div14, button1);
      append(button1, t26);
      append(section, t27);
      if (if_block1) if_block1.m(section, null);
      insert(target, t28, anchor);
      if (if_block2) if_block2.m(target, anchor);
      insert(target, if_block2_anchor, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*openAddPort*/
            ctx[15]
          ),
          listen(
            button1,
            "click",
            /*removePorts*/
            ctx[20]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type(ctx2)) && if_block0) {
        if_block0.p(ctx2, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx2);
        if (if_block0) {
          if_block0.c();
          if_block0.m(div13, null);
        }
      }
      if (dirty[0] & /*selectedConnectorIds, portSaving*/
      768 && button1_disabled_value !== (button1_disabled_value = !/*selectedConnectorIds*/
      ctx2[9].length || /*portSaving*/
      ctx2[8])) {
        button1.disabled = button1_disabled_value;
      }
      if (
        /*portRemoveStatus*/
        ctx2[6]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_5$2(ctx2);
          if_block1.c();
          if_block1.m(section, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (
        /*showAddPortOverlay*/
        ctx2[10]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block$3(ctx2);
          if_block2.c();
          if_block2.m(if_block2_anchor.parentNode, if_block2_anchor);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
        detach(t28);
        detach(if_block2_anchor);
      }
      if_block0.d();
      if (if_block1) if_block1.d();
      if (if_block2) if_block2.d(detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
const VSP_DEVNAME_MAX = 64;
function instance$3($$self, $$props, $$invalidate) {
  let { connectors = [] } = $$props;
  let { keyers = [] } = $$props;
  let { devices = [] } = $$props;
  let { metadata = null } = $$props;
  let { reloadData = async () => {
  } } = $$props;
  const portTypeOptions = ["VSP", "TCP"];
  let portForm = {
    type: "VSP",
    serial: "",
    channel: "R1",
    devname: "",
    maxcon: 1,
    ptt_rts: false,
    ptt_dtr: false,
    remote_access: false
  };
  let portStatus = "";
  let portStatusKind = "success";
  let portRemoveStatus = "";
  let portRemoveStatusKind = "success";
  let portSaving = false;
  let portStatusTimer;
  let selectedConnectorIds = [];
  let showAddPortOverlay = false;
  let devnameTouched = false;
  const keyerFlagsForSerial = (serial) => {
    const cfg = keyers.find((k) => k.serial === serial);
    if (!cfg || cfg.type == null || !(metadata == null ? void 0 : metadata.devicetypes)) return [];
    const devType = metadata.devicetypes.find((d) => d.type === cfg.type);
    return (devType == null ? void 0 : devType.flags) || [];
  };
  const deviceNameForSerial = (serial) => {
    const dev = devices.find((d) => d.serial === serial);
    return (dev == null ? void 0 : dev.name) || serial || "—";
  };
  const channelOptionsForSerial = (serial) => {
    if (!serial) return [];
    const flags = keyerFlagsForSerial(serial);
    const opts = [];
    if (flags.includes("HAS_R1")) opts.push("CAT1");
    if (flags.includes("HAS_R2")) opts.push("CAT2");
    if (flags.includes("HAS_FLAGS_CHANNEL")) opts.push("PTT1");
    if (flags.includes("HAS_R2") || flags.includes("HAS_FSK2")) opts.push("PTT2");
    if (flags.includes("HAS_AUX")) opts.push("AUX");
    if (flags.includes("HAS_WINKEY")) opts.push("WK");
    if (flags.includes("HAS_FSK1")) opts.push("FSK1");
    if (flags.includes("HAS_FSK2")) opts.push("FSK2");
    if (flags.includes("HAS_MCP_SUPPORT")) opts.push("MCP");
    if (flags.includes("HAS_ROTATOR_SUPPORT")) opts.push("ROTATOR");
    return opts;
  };
  const displayPortChannel = (channel) => {
    if (!channel) return "—";
    if (channel === "R1") return "CAT1";
    if (channel === "R2") return "CAT2";
    return channel;
  };
  const suggestVspDevname = (channel, existing) => {
    if (!channel) return "";
    const base = displayPortChannel(channel).toLowerCase();
    const used = new Set(existing.filter((c) => c.type === "VSP").map((c) => c.devname));
    if (!used.has(base)) return base;
    let n = 2;
    while (used.has(`${base}_${n}`)) n++;
    return `${base}_${n}`;
  };
  const openAddPort = () => {
    $$invalidate(3, devnameTouched = false);
    $$invalidate(10, showAddPortOverlay = true);
  };
  const checkMark = (value) => value ? "✓" : "—";
  const vspDevnameValid = (name) => typeof name === "string" && name.length <= VSP_DEVNAME_MAX && /^[A-Za-z0-9][A-Za-z0-9_.-]*$/.test(name);
  const buildConnectorPayload = () => {
    if (!portForm.serial || !portForm.channel || !portForm.type || !portForm.devname) return null;
    const base = {
      serial: portForm.serial,
      channel: portForm.channel,
      type: portForm.type,
      devname: String(portForm.devname)
    };
    if (portForm.maxcon) base.maxcon = Number(portForm.maxcon);
    if (portForm.type === "VSP") {
      base.ptt_rts = portForm.ptt_rts ? 1 : 0;
      base.ptt_dtr = portForm.ptt_dtr ? 1 : 0;
    } else if (portForm.type === "TCP") {
      base.remote_access = portForm.remote_access ? 1 : 0;
    }
    return base;
  };
  const toggleConnectorSelection = (id) => {
    if (id == null) return;
    if (selectedConnectorIds.includes(id)) {
      $$invalidate(9, selectedConnectorIds = selectedConnectorIds.filter((v) => v !== id));
    } else {
      $$invalidate(9, selectedConnectorIds = [...selectedConnectorIds, id]);
    }
  };
  const applyPort = async () => {
    const connector = buildConnectorPayload();
    if (!connector) {
      $$invalidate(4, portStatus = "Please fill out all required fields.");
      $$invalidate(5, portStatusKind = "error");
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => $$invalidate(4, portStatus = ""), 3e3);
      return;
    }
    if (connector.type === "VSP" && !vspDevnameValid(connector.devname)) {
      $$invalidate(4, portStatus = `Device name must be 1 to ${VSP_DEVNAME_MAX} characters from A-Z a-z 0-9 _ - . and start with a letter or digit.`);
      $$invalidate(5, portStatusKind = "error");
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => $$invalidate(4, portStatus = ""), 6e3);
      return;
    }
    $$invalidate(8, portSaving = true);
    $$invalidate(4, portStatus = "");
    $$invalidate(5, portStatusKind = "success");
    try {
      const res = await fetch("/api/v1/config/connectors", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify(connector)
      });
      if (!res.ok) throw new Error(`config/connectors ${res.status}`);
      await reloadData();
      $$invalidate(4, portStatus = "Port saved.");
      $$invalidate(5, portStatusKind = "success");
      $$invalidate(10, showAddPortOverlay = false);
    } catch (err) {
      $$invalidate(4, portStatus = (err == null ? void 0 : err.message) || "Failed to save port.");
      $$invalidate(5, portStatusKind = "error");
    } finally {
      $$invalidate(8, portSaving = false);
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => $$invalidate(4, portStatus = ""), 3e3);
    }
  };
  const updatePortSerial = (serial) => {
    const opts = channelOptionsForSerial(serial);
    const nextChannel = opts.includes(portForm.channel) ? portForm.channel : opts[0] || "";
    $$invalidate(2, portForm = {
      ...portForm,
      serial,
      channel: nextChannel
    });
  };
  const removePorts = async () => {
    if (!selectedConnectorIds.length) return;
    $$invalidate(8, portSaving = true);
    $$invalidate(6, portRemoveStatus = "");
    $$invalidate(7, portRemoveStatusKind = "success");
    try {
      await Promise.all(selectedConnectorIds.map(async (id) => {
        const res = await fetch(`/api/v1/config/connectors/${id}`, {
          method: "DELETE",
          headers: { Accept: "application/json" }
        });
        if (!res.ok) throw new Error(`config/connectors/${id} ${res.status}`);
      }));
      await reloadData();
      $$invalidate(9, selectedConnectorIds = []);
      $$invalidate(6, portRemoveStatus = "Port(s) removed.");
      $$invalidate(7, portRemoveStatusKind = "success");
    } catch (err) {
      $$invalidate(6, portRemoveStatus = (err == null ? void 0 : err.message) || "Failed to remove port(s).");
      $$invalidate(7, portRemoveStatusKind = "error");
    } finally {
      $$invalidate(8, portSaving = false);
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(
        () => {
          $$invalidate(6, portRemoveStatus = "");
        },
        3e3
      );
    }
  };
  const $$binding_groups = [[]];
  const change_handler = (c) => toggleConnectorSelection(c.id);
  const click_handler = () => $$invalidate(10, showAddPortOverlay = false);
  function input_change_handler() {
    portForm.type = this.__value;
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  const change_handler_1 = () => $$invalidate(3, devnameTouched = false);
  const change_handler_2 = (e) => updatePortSerial(e.target.value);
  function select1_change_handler() {
    portForm.channel = select_value(this);
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  function input0_input_handler() {
    portForm.devname = this.value;
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  const input_handler = () => $$invalidate(3, devnameTouched = true);
  function input1_change_handler() {
    portForm.ptt_rts = this.checked;
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  function input2_change_handler() {
    portForm.ptt_dtr = this.checked;
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  function input0_input_handler_1() {
    portForm.devname = to_number(this.value);
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  const input_handler_1 = () => $$invalidate(3, devnameTouched = true);
  function input1_change_handler_1() {
    portForm.remote_access = this.checked;
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  function input_input_handler() {
    portForm.maxcon = to_number(this.value);
    $$invalidate(2, portForm), $$invalidate(1, keyers), $$invalidate(3, devnameTouched), $$invalidate(0, connectors);
    $$invalidate(1, keyers);
  }
  const click_handler_1 = () => $$invalidate(10, showAddPortOverlay = false);
  const click_handler_2 = () => $$invalidate(10, showAddPortOverlay = false);
  $$self.$$set = ($$props2) => {
    if ("connectors" in $$props2) $$invalidate(0, connectors = $$props2.connectors);
    if ("keyers" in $$props2) $$invalidate(1, keyers = $$props2.keyers);
    if ("devices" in $$props2) $$invalidate(21, devices = $$props2.devices);
    if ("metadata" in $$props2) $$invalidate(22, metadata = $$props2.metadata);
    if ("reloadData" in $$props2) $$invalidate(23, reloadData = $$props2.reloadData);
  };
  $$self.$$.update = () => {
    if ($$self.$$.dirty[0] & /*portForm, keyers*/
    6) {
      if (!portForm.serial && keyers.length) {
        const serial = keyers[0].serial || "";
        const opts = channelOptionsForSerial(serial);
        $$invalidate(2, portForm = {
          ...portForm,
          serial,
          channel: opts[0] || ""
        });
      }
    }
    if ($$self.$$.dirty[0] & /*devnameTouched, portForm, connectors*/
    13) {
      if (!devnameTouched) {
        const devname = portForm.type === "VSP" ? suggestVspDevname(portForm.channel, connectors) : "";
        if (portForm.devname !== devname) $$invalidate(2, portForm = { ...portForm, devname });
      }
    }
  };
  return [
    connectors,
    keyers,
    portForm,
    devnameTouched,
    portStatus,
    portStatusKind,
    portRemoveStatus,
    portRemoveStatusKind,
    portSaving,
    selectedConnectorIds,
    showAddPortOverlay,
    portTypeOptions,
    deviceNameForSerial,
    channelOptionsForSerial,
    displayPortChannel,
    openAddPort,
    checkMark,
    toggleConnectorSelection,
    applyPort,
    updatePortSerial,
    removePorts,
    devices,
    metadata,
    reloadData,
    change_handler,
    click_handler,
    input_change_handler,
    $$binding_groups,
    change_handler_1,
    change_handler_2,
    select1_change_handler,
    input0_input_handler,
    input_handler,
    input1_change_handler,
    input2_change_handler,
    input0_input_handler_1,
    input_handler_1,
    input1_change_handler_1,
    input_input_handler,
    click_handler_1,
    click_handler_2
  ];
}
class DaemonPorts extends SvelteComponent {
  constructor(options) {
    super();
    init(
      this,
      options,
      instance$3,
      create_fragment$3,
      safe_not_equal,
      {
        connectors: 0,
        keyers: 1,
        devices: 21,
        metadata: 22,
        reloadData: 23
      },
      null,
      [-1, -1]
    );
  }
}
function create_if_block$2(ctx) {
  let div;
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(
        /*loglevelStatus*/
        ctx[2]
      );
      attr(div, "class", div_class_value = `inline-status ${/*loglevelStatusKind*/
      ctx[3] === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty & /*loglevelStatus*/
      4) set_data(
        t,
        /*loglevelStatus*/
        ctx2[2]
      );
      if (dirty & /*loglevelStatusKind*/
      8 && div_class_value !== (div_class_value = `inline-status ${/*loglevelStatusKind*/
      ctx2[3] === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_fragment$2(ctx) {
  let section;
  let div0;
  let t1;
  let div4;
  let div3;
  let div1;
  let t3;
  let div2;
  let select;
  let option0;
  let option1;
  let option2;
  let option3;
  let option4;
  let option5;
  let t10;
  let mounted;
  let dispose;
  let if_block = (
    /*loglevelStatus*/
    ctx[2] && create_if_block$2(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Logging";
      t1 = space();
      div4 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Log Level:";
      t3 = space();
      div2 = element("div");
      select = element("select");
      option0 = element("option");
      option0.textContent = "CRIT";
      option1 = element("option");
      option1.textContent = "ERROR";
      option2 = element("option");
      option2.textContent = "WARN";
      option3 = element("option");
      option3.textContent = "INFO";
      option4 = element("option");
      option4.textContent = "DEBUG0";
      option5 = element("option");
      option5.textContent = "DEBUG1";
      t10 = space();
      if (if_block) if_block.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      option0.__value = "CRIT";
      set_input_value(option0, option0.__value);
      option1.__value = "ERROR";
      set_input_value(option1, option1.__value);
      option2.__value = "WARN";
      set_input_value(option2, option2.__value);
      option3.__value = "INFO";
      set_input_value(option3, option3.__value);
      option4.__value = "DEBUG0";
      set_input_value(option4, option4.__value);
      option5.__value = "DEBUG1";
      set_input_value(option5, option5.__value);
      attr(select, "class", "select");
      select.disabled = /*loglevelSaving*/
      ctx[0];
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div4);
      append(div4, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select);
      append(select, option0);
      append(select, option1);
      append(select, option2);
      append(select, option3);
      append(select, option4);
      append(select, option5);
      select_option(
        select,
        /*loglevel*/
        ctx[1]
      );
      append(div4, t10);
      if (if_block) if_block.m(div4, null);
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*applyLoglevel*/
          ctx[4]
        );
        mounted = true;
      }
    },
    p(ctx2, [dirty]) {
      if (dirty & /*loglevel*/
      2) {
        select_option(
          select,
          /*loglevel*/
          ctx2[1]
        );
      }
      if (dirty & /*loglevelSaving*/
      1) {
        select.disabled = /*loglevelSaving*/
        ctx2[0];
      }
      if (
        /*loglevelStatus*/
        ctx2[2]
      ) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block$2(ctx2);
          if_block.c();
          if_block.m(div4, null);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      if (if_block) if_block.d();
      mounted = false;
      dispose();
    }
  };
}
function instance$2($$self, $$props, $$invalidate) {
  let { daemonCfg } = $$props;
  let loglevel = (daemonCfg == null ? void 0 : daemonCfg.loglevel) || "";
  let loglevelStatus = "";
  let loglevelStatusKind = "success";
  let loglevelSaving = false;
  let loglevelTimer;
  const applyLoglevel = async (e) => {
    const selectedLevel = e.target.value;
    if (!selectedLevel) return;
    $$invalidate(0, loglevelSaving = true);
    $$invalidate(2, loglevelStatus = "");
    $$invalidate(3, loglevelStatusKind = "success");
    try {
      const res = await fetch("/api/v1/config/daemon", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ loglevel: selectedLevel })
      });
      if (!res.ok) throw new Error(`config/daemon ${res.status}`);
      const data = await res.json();
      $$invalidate(5, daemonCfg = data);
      $$invalidate(1, loglevel = (data == null ? void 0 : data.loglevel) || selectedLevel);
      $$invalidate(2, loglevelStatus = "Log level updated.");
      $$invalidate(3, loglevelStatusKind = "success");
    } catch (err) {
      $$invalidate(2, loglevelStatus = (err == null ? void 0 : err.message) || "Failed to update log level.");
      $$invalidate(3, loglevelStatusKind = "error");
    } finally {
      $$invalidate(0, loglevelSaving = false);
      clearTimeout(loglevelTimer);
      loglevelTimer = setTimeout(() => $$invalidate(2, loglevelStatus = ""), 3e3);
    }
  };
  $$self.$$set = ($$props2) => {
    if ("daemonCfg" in $$props2) $$invalidate(5, daemonCfg = $$props2.daemonCfg);
  };
  $$self.$$.update = () => {
    if ($$self.$$.dirty & /*loglevelSaving, daemonCfg*/
    33) {
      if (!loglevelSaving && (daemonCfg == null ? void 0 : daemonCfg.loglevel)) $$invalidate(1, loglevel = daemonCfg.loglevel);
    }
  };
  return [
    loglevelSaving,
    loglevel,
    loglevelStatus,
    loglevelStatusKind,
    applyLoglevel,
    daemonCfg
  ];
}
class DaemonSettings extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$2, create_fragment$2, safe_not_equal, { daemonCfg: 5 });
  }
}
async function apiGet(url) {
  const res = await fetch(url, { headers: { Accept: "application/json" } });
  if (!res.ok) throw new Error(`${url} ${res.status}`);
  return res.json();
}
async function loadAllData() {
  const [runtime, daemonCfg, devicesRsp, configConnectors, configDevicesRsp, metadata] = await Promise.all([
    apiGet("/api/v1/runtime"),
    apiGet("/api/v1/config/daemon"),
    apiGet("/api/v1/devices"),
    apiGet("/api/v1/config/connectors"),
    apiGet("/api/v1/config/devices"),
    apiGet("/api/v1/metadata")
  ]);
  return {
    runtime,
    daemonCfg,
    devices: devicesRsp.devices || [],
    configDevices: configDevicesRsp.devices || [],
    connectors: configConnectors || [],
    metadata
  };
}
function get_each_context_90(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[705] = list[i];
  return child_ctx;
}
function get_each_context_76(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[672] = list[i];
  return child_ctx;
}
function get_each_context_77(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[644] = list[i];
  return child_ctx;
}
function get_each_context_78(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[647] = list[i];
  const constants_0 = (
    /*activeAllObjs*/
    child_ctx[49].find(function func(...args) {
      return (
        /*func*/
        ctx[417](
          /*ref*/
          child_ctx[647],
          ...args
        )
      );
    })
  );
  child_ctx[677] = constants_0;
  return child_ctx;
}
function get_each_context_79(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[680] = list[i];
  return child_ctx;
}
function get_each_context_80(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[680] = list[i];
  return child_ctx;
}
function get_each_context_81(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[644] = list[i];
  return child_ctx;
}
function get_if_ctx_9(ctx) {
  var _a, _b, _c;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smBndEditForms*/
    ((_c = (_b = (_a = child_ctx[33][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*bnd*/
      child_ctx[672].id
    ]) == null ? void 0 : _b.refs) == null ? void 0 : _c[
      /*ref*/
      child_ctx[647].id
    ]) || {}
  );
  child_ctx[650] = constants_0;
  return child_ctx;
}
function get_each_context_86(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[687] = list[i];
  return child_ctx;
}
function get_each_context_87(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[687] = list[i];
  return child_ctx;
}
function get_each_context_88(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[692] = list[i];
  return child_ctx;
}
function get_each_context_89(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[692] = list[i];
  return child_ctx;
}
function get_each_context_82(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[687] = list[i];
  return child_ctx;
}
function get_each_context_83(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[687] = list[i];
  return child_ctx;
}
function get_each_context_84(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[692] = list[i];
  return child_ctx;
}
function get_each_context_85(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[692] = list[i];
  return child_ctx;
}
function get_if_ctx_10(ctx) {
  var _a;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smBndEditForms*/
    ((_a = child_ctx[33][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*bnd*/
      child_ctx[672].id
    ]) || {}
  );
  child_ctx[630] = constants_0;
  return child_ctx;
}
function get_each_context_72(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[656] = list[i];
  return child_ctx;
}
function get_each_context_73(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[644] = list[i];
  return child_ctx;
}
function get_each_context_74(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[647] = list[i];
  return child_ctx;
}
function get_each_context_75(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[644] = list[i];
  return child_ctx;
}
function get_if_ctx_6(ctx) {
  var _a, _b, _c;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smGrpEditForms*/
    ((_c = (_b = (_a = child_ctx[28][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*grp*/
      child_ctx[656].id
    ]) == null ? void 0 : _b.refs) == null ? void 0 : _c[
      /*ref*/
      child_ctx[647].id
    ]) || {}
  );
  child_ctx[650] = constants_0;
  return child_ctx;
}
function get_if_ctx_7(ctx) {
  var _a;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smGrpEditForms*/
    ((_a = child_ctx[28][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*grp*/
      child_ctx[656].id
    ]) || {}
  );
  child_ctx[630] = constants_0;
  return child_ctx;
}
function get_each_context_68(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[641] = list[i];
  return child_ctx;
}
function get_each_context_69(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[644] = list[i];
  return child_ctx;
}
function get_each_context_70(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[647] = list[i];
  return child_ctx;
}
function get_each_context_71(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[644] = list[i];
  return child_ctx;
}
function get_if_ctx_3(ctx) {
  var _a, _b, _c;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smVrEditForms*/
    ((_c = (_b = (_a = child_ctx[23][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*vr*/
      child_ctx[641].id
    ]) == null ? void 0 : _b.refs) == null ? void 0 : _c[
      /*ref*/
      child_ctx[647].id
    ]) || {}
  );
  child_ctx[650] = constants_0;
  return child_ctx;
}
function get_if_ctx_4(ctx) {
  var _a;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smVrEditForms*/
    ((_a = child_ctx[23][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*vr*/
      child_ctx[641].id
    ]) || {}
  );
  child_ctx[630] = constants_0;
  return child_ctx;
}
function get_each_context_63(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[624] = list[i];
  child_ctx[625] = list;
  child_ctx[626] = i;
  return child_ctx;
}
function get_if_ctx(ctx) {
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smAntAddForm*/
    child_ctx[19][
      /*activeSerial*/
      child_ctx[0]
    ] || /*smAntNewForm*/
    child_ctx[118]()
  );
  child_ctx[623] = constants_0;
  return child_ctx;
}
function get_each_context_64(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[627] = list[i];
  return child_ctx;
}
function get_each_context_66(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[624] = list[i];
  return child_ctx;
}
function get_each_context_65(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[624] = list[i];
  return child_ctx;
}
function get_if_ctx_1(ctx) {
  var _a;
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*smAntEditForms*/
    ((_a = child_ctx[18][
      /*activeSerial*/
      child_ctx[0]
    ]) == null ? void 0 : _a[
      /*ant*/
      child_ctx[627].id
    ]) || {}
  );
  child_ctx[630] = constants_0;
  return child_ctx;
}
function get_each_context_67(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[624] = list[i];
  return child_ctx;
}
function get_each_context_61(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[606] = list[i];
  return child_ctx;
}
function get_each_context_62(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_56(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[606] = list[i];
  return child_ctx;
}
function get_each_context_57(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_58(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_59(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_60(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_54(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[601] = list[i];
  return child_ctx;
}
function get_each_context_55(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[601] = list[i];
  return child_ctx;
}
function get_each_context_51(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_52(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_53(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_47(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_48(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[588] = list[i];
  return child_ctx;
}
function get_each_context_49(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_50(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_37(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[547] = list[i];
  return child_ctx;
}
function get_each_context_38(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_39(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_40(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_41(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_42(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[547] = list[i];
  return child_ctx;
}
function get_each_context_43(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_44(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_45(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_46(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_32(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[547] = list[i];
  return child_ctx;
}
function get_each_context_33(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_34(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_35(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_36(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_27(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_28(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[547] = list[i];
  return child_ctx;
}
function get_each_context_29(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_30(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_31(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_21(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_22(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_23(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_24(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_25(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_26(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_15(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_16(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_17(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_18(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_19(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_20(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_12(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_13(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_14(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_2(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_3(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[496] = list[i];
  return child_ctx;
}
function get_each_context_4(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_5(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_6(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_7(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_8(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[496] = list[i];
  return child_ctx;
}
function get_each_context_9(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_10(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_11(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_each_context_1(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[489] = list[i];
  return child_ctx;
}
function get_if_ctx_11(ctx) {
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*activeBnds*/
    child_ctx[48]
  );
  child_ctx[665] = constants_0;
  const constants_1 = (
    /*smBndMode*/
    child_ctx[31][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[666] = constants_1;
  const constants_2 = (
    /*smBndAddRef*/
    child_ctx[35][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[667] = constants_2;
  const constants_3 = (
    /*activeBpfOutputs*/
    child_ctx[47]
  );
  child_ctx[668] = constants_3;
  const constants_4 = (
    /*activeSeqOutputs*/
    child_ctx[46]
  );
  child_ctx[669] = constants_4;
  const constants_5 = (
    /*activeAntOutputs*/
    child_ctx[45]
  );
  child_ctx[670] = constants_5;
  const constants_6 = (
    /*activeAntsAndGroups*/
    child_ctx[44]
  );
  child_ctx[671] = constants_6;
  return child_ctx;
}
function get_if_ctx_8(ctx) {
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*activeGrps*/
    child_ctx[50]
  );
  child_ctx[653] = constants_0;
  const constants_1 = (
    /*smGrpMode*/
    child_ctx[26][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[654] = constants_1;
  const constants_2 = (
    /*smGrpAddAnt*/
    child_ctx[30][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[655] = constants_2;
  const constants_3 = (
    /*activeAllObjs*/
    child_ctx[49]
  );
  child_ctx[640] = constants_3;
  return child_ctx;
}
function get_if_ctx_5(ctx) {
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*activeVrs*/
    child_ctx[51]
  );
  child_ctx[637] = constants_0;
  const constants_1 = (
    /*smVrMode*/
    child_ctx[21][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[638] = constants_1;
  const constants_2 = (
    /*smVrAddAnt*/
    child_ctx[25][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[639] = constants_2;
  const constants_3 = (
    /*activeAllObjs*/
    child_ctx[49]
  );
  child_ctx[640] = constants_3;
  return child_ctx;
}
function get_if_ctx_2(ctx) {
  const child_ctx = ctx.slice();
  const constants_0 = (
    /*activeAnts*/
    child_ctx[53]
  );
  child_ctx[621] = constants_0;
  const constants_1 = (
    /*activeAntOutCols*/
    child_ctx[52]
  );
  child_ctx[622] = constants_1;
  const constants_2 = (
    /*smAntMode*/
    child_ctx[17][
      /*activeSerial*/
      child_ctx[0]
    ]
  );
  child_ctx[547] = constants_2;
  return child_ctx;
}
function create_else_block_22(ctx) {
  let section;
  let div0;
  let t1;
  let div2;
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = `${/*activeKeyerMenuTitle*/
      ctx[159]()}`;
      t1 = space();
      div2 = element("div");
      div2.innerHTML = `<div class="placeholder">Keyer page placeholder.</div>`;
      attr(div0, "class", "section-title");
      attr(div2, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div2);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
      }
    }
  };
}
function create_if_block_119(ctx) {
  let section;
  let div0;
  let t1;
  let div2;
  let t2;
  let div1;
  let button;
  let t4;
  let mounted;
  let dispose;
  let each_value_90 = ensure_array_like(Object.entries(
    /*allParamForm*/
    ctx[13][
      /*activeSerial*/
      ctx[0]
    ] || {}
  ));
  let each_blocks = [];
  for (let i = 0; i < each_value_90.length; i += 1) {
    each_blocks[i] = create_each_block_90(get_each_context_90(ctx, each_value_90, i));
  }
  let if_block = (
    /*allParamStatus*/
    ctx[14][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_120(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "All Keyer Parameters";
      t1 = space();
      div2 = element("div");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t2 = space();
      div1 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t4 = space();
      if (if_block) if_block.c();
      attr(div0, "class", "section-title");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div1, "class", "button-row");
      attr(div2, "class", "panel all-params-panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div2, null);
        }
      }
      append(div2, t2);
      append(div2, div1);
      append(div1, button);
      append(div2, t4);
      if (if_block) if_block.m(div2, null);
      if (!mounted) {
        dispose = listen(
          button,
          "click",
          /*click_handler_49*/
          ctx[436]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*allParamForm, activeSerial*/
      8193 | dirty[2] & /*updateAllParamForm*/
      1073741824) {
        each_value_90 = ensure_array_like(Object.entries(
          /*allParamForm*/
          ctx2[13][
            /*activeSerial*/
            ctx2[0]
          ] || {}
        ));
        let i;
        for (i = 0; i < each_value_90.length; i += 1) {
          const child_ctx = get_each_context_90(ctx2, each_value_90, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_90(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div2, t2);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_90.length;
      }
      if (
        /*allParamStatus*/
        ctx2[14][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block_120(ctx2);
          if_block.c();
          if_block.m(div2, null);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks, detaching);
      if (if_block) if_block.d();
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_93(ctx) {
  let section;
  let div0;
  let t1;
  let div2;
  let table;
  let thead;
  let tr;
  let th0;
  let t2;
  let th1;
  let t4;
  let th2;
  let t6;
  let th3;
  let t8;
  let th4;
  let t10;
  let th5;
  let t12;
  let th6;
  let t14;
  let th7;
  let t16;
  let t17;
  let t18;
  let tbody;
  let each_blocks = [];
  let each_1_lookup = /* @__PURE__ */ new Map();
  let t19;
  let t20;
  let div1;
  let t21;
  let if_block0 = (
    /*bpfOuts*/
    ctx[668].length > 0 && create_if_block_118()
  );
  let if_block1 = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_117()
  );
  let each_value_76 = ensure_array_like(
    /*bnds*/
    ctx[665]
  );
  const get_key = (ctx2) => (
    /*bnd*/
    ctx2[672].id
  );
  for (let i = 0; i < each_value_76.length; i += 1) {
    let child_ctx = get_each_context_76(ctx, each_value_76, i);
    let key = get_key(child_ctx);
    each_1_lookup.set(key, each_blocks[i] = create_each_block_76(key, child_ctx));
  }
  let if_block2 = (
    /*bndMode*/
    ctx[666] === "add" && create_if_block_98(ctx)
  );
  function select_block_type_22(ctx2, dirty) {
    if (
      /*bndAddRefState*/
      ctx2[667]
    ) return create_if_block_95;
    if (
      /*bndMode*/
      ctx2[666] === "add"
    ) return create_if_block_96;
    if (
      /*bndMode*/
      ctx2[666] === "edit"
    ) return create_if_block_97;
    return create_else_block_16;
  }
  let current_block_type = select_block_type_22(ctx);
  let if_block3 = current_block_type(ctx);
  let if_block4 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_94(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Band List";
      t1 = space();
      div2 = element("div");
      table = element("table");
      thead = element("thead");
      tr = element("tr");
      th0 = element("th");
      t2 = space();
      th1 = element("th");
      th1.textContent = "ID";
      t4 = space();
      th2 = element("th");
      th2.textContent = "Name";
      t6 = space();
      th3 = element("th");
      th3.textContent = "Frequency Range";
      t8 = space();
      th4 = element("th");
      th4.textContent = "Code";
      t10 = space();
      th5 = element("th");
      th5.textContent = "PA";
      t12 = space();
      th6 = element("th");
      th6.textContent = "KeyOut";
      t14 = space();
      th7 = element("th");
      th7.textContent = "Ant";
      t16 = space();
      if (if_block0) if_block0.c();
      t17 = space();
      if (if_block1) if_block1.c();
      t18 = space();
      tbody = element("tbody");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t19 = space();
      if (if_block2) if_block2.c();
      t20 = space();
      div1 = element("div");
      if_block3.c();
      t21 = space();
      if (if_block4) if_block4.c();
      attr(div0, "class", "section-title");
      attr(th3, "colspan", "2");
      attr(table, "class", "ant-table");
      attr(div1, "class", "button-row");
      set_style(div1, "margin-top", "8px");
      attr(div2, "class", "panel");
      set_style(div2, "overflow-x", "auto");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div2);
      append(div2, table);
      append(table, thead);
      append(thead, tr);
      append(tr, th0);
      append(tr, t2);
      append(tr, th1);
      append(tr, t4);
      append(tr, th2);
      append(tr, t6);
      append(tr, th3);
      append(tr, t8);
      append(tr, th4);
      append(tr, t10);
      append(tr, th5);
      append(tr, t12);
      append(tr, th6);
      append(tr, t14);
      append(tr, th7);
      append(tr, t16);
      if (if_block0) if_block0.m(tr, null);
      append(tr, t17);
      if (if_block1) if_block1.m(tr, null);
      append(table, t18);
      append(table, tbody);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tbody, null);
        }
      }
      append(tbody, t19);
      if (if_block2) if_block2.m(tbody, null);
      append(div2, t20);
      append(div2, div1);
      if_block3.m(div1, null);
      append(div2, t21);
      if (if_block4) if_block4.m(div2, null);
    },
    p(ctx2, dirty) {
      if (
        /*bpfOuts*/
        ctx2[668].length > 0
      ) {
        if (if_block0) ;
        else {
          if_block0 = create_if_block_118();
          if_block0.c();
          if_block0.m(tr, t17);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*seqOuts*/
        ctx2[669].length > 0
      ) {
        if (if_block1) ;
        else {
          if_block1 = create_if_block_117();
          if_block1.c();
          if_block1.m(tr, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeSeqOutputs, smBndAddRef, activeAntsAndGroups, activeBnds, smBndMode, activeBpfOutputs, activeAntOutputs, activeAllObjs, smBndEditForms, smBndSelected*/
      516125 | dirty[4] & /*smObjTypedDisplay*/
      16777216 | dirty[5] & /*smBndCancelAddRef, smBndSaveAddRef, smBndStartAddRef, smBndToggleSelect*/
      15) {
        each_value_76 = ensure_array_like(
          /*bnds*/
          ctx2[665]
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx2, each_value_76, each_1_lookup, tbody, destroy_block, create_each_block_76, t19, get_each_context_76);
      }
      if (
        /*bndMode*/
        ctx2[666] === "add"
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_98(ctx2);
          if_block2.c();
          if_block2.m(tbody, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (current_block_type === (current_block_type = select_block_type_22(ctx2)) && if_block3) {
        if_block3.p(ctx2, dirty);
      } else {
        if_block3.d(1);
        if_block3 = current_block_type(ctx2);
        if (if_block3) {
          if_block3.c();
          if_block3.m(div1, null);
        }
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block4) {
          if_block4.p(ctx2, dirty);
        } else {
          if_block4 = create_if_block_94(ctx2);
          if_block4.c();
          if_block4.m(div2, null);
        }
      } else if (if_block4) {
        if_block4.d(1);
        if_block4 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d();
      }
      if (if_block2) if_block2.d();
      if_block3.d();
      if (if_block4) if_block4.d();
    }
  };
}
function create_if_block_81(ctx) {
  let section;
  let div0;
  let t1;
  let div2;
  let table;
  let thead;
  let t12;
  let tbody;
  let each_blocks = [];
  let each_1_lookup = /* @__PURE__ */ new Map();
  let t13;
  let t14;
  let div1;
  let t15;
  let each_value_72 = ensure_array_like(
    /*grps*/
    ctx[653]
  );
  const get_key = (ctx2) => (
    /*grp*/
    ctx2[656].id
  );
  for (let i = 0; i < each_value_72.length; i += 1) {
    let child_ctx = get_each_context_72(ctx, each_value_72, i);
    let key = get_key(child_ctx);
    each_1_lookup.set(key, each_blocks[i] = create_each_block_72(key, child_ctx));
  }
  let if_block0 = (
    /*grpMode*/
    ctx[654] === "add" && create_if_block_86(ctx)
  );
  function select_block_type_16(ctx2, dirty) {
    if (
      /*grpAddAntState*/
      ctx2[655]
    ) return create_if_block_83;
    if (
      /*grpMode*/
      ctx2[654] === "add"
    ) return create_if_block_84;
    if (
      /*grpMode*/
      ctx2[654] === "edit"
    ) return create_if_block_85;
    return create_else_block_11;
  }
  let current_block_type = select_block_type_16(ctx);
  let if_block1 = current_block_type(ctx);
  let if_block2 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_82(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Antenna Groups List";
      t1 = space();
      div2 = element("div");
      table = element("table");
      thead = element("thead");
      thead.innerHTML = `<tr><th></th> <th>ID</th> <th>Label</th> <th>Name</th> <th>RX-Only</th> <th>Antenna</th></tr>`;
      t12 = space();
      tbody = element("tbody");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t13 = space();
      if (if_block0) if_block0.c();
      t14 = space();
      div1 = element("div");
      if_block1.c();
      t15 = space();
      if (if_block2) if_block2.c();
      attr(div0, "class", "section-title");
      attr(table, "class", "ant-table");
      attr(div1, "class", "button-row");
      set_style(div1, "margin-top", "8px");
      attr(div2, "class", "panel");
      set_style(div2, "overflow-x", "auto");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div2);
      append(div2, table);
      append(table, thead);
      append(table, t12);
      append(table, tbody);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tbody, null);
        }
      }
      append(tbody, t13);
      if (if_block0) if_block0.m(tbody, null);
      append(div2, t14);
      append(div2, div1);
      if_block1.m(div1, null);
      append(div2, t15);
      if (if_block2) if_block2.m(div2, null);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smGrpAddAnt, activeSerial, smGrpEditForms, smGrpMode, smGrpSelected*/
      1946157057 | dirty[1] & /*activeAllObjs, activeGrps*/
      786432 | dirty[4] & /*smObjDisplay, smObjRxOnly, smGrpToggleSelect, smGrpStartAddAnt*/
      3145740) {
        each_value_72 = ensure_array_like(
          /*grps*/
          ctx2[653]
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx2, each_value_72, each_1_lookup, tbody, destroy_block, create_each_block_72, t13, get_each_context_72);
      }
      if (
        /*grpMode*/
        ctx2[654] === "add"
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_86(ctx2);
          if_block0.c();
          if_block0.m(tbody, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (current_block_type === (current_block_type = select_block_type_16(ctx2)) && if_block1) {
        if_block1.p(ctx2, dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type(ctx2);
        if (if_block1) {
          if_block1.c();
          if_block1.m(div1, null);
        }
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_82(ctx2);
          if_block2.c();
          if_block2.m(div2, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d();
      }
      if (if_block0) if_block0.d();
      if_block1.d();
      if (if_block2) if_block2.d();
    }
  };
}
function create_if_block_69(ctx) {
  let section;
  let div0;
  let t1;
  let div2;
  let table;
  let thead;
  let t16;
  let tbody;
  let each_blocks = [];
  let each_1_lookup = /* @__PURE__ */ new Map();
  let t17;
  let t18;
  let div1;
  let t19;
  let each_value_68 = ensure_array_like(
    /*vrs*/
    ctx[637]
  );
  const get_key = (ctx2) => (
    /*vr*/
    ctx2[641].id
  );
  for (let i = 0; i < each_value_68.length; i += 1) {
    let child_ctx = get_each_context_68(ctx, each_value_68, i);
    let key = get_key(child_ctx);
    each_1_lookup.set(key, each_blocks[i] = create_each_block_68(key, child_ctx));
  }
  let if_block0 = (
    /*vrMode*/
    ctx[638] === "add" && create_if_block_74(ctx)
  );
  function select_block_type_11(ctx2, dirty) {
    if (
      /*vrAddAntState*/
      ctx2[639]
    ) return create_if_block_71;
    if (
      /*vrMode*/
      ctx2[638] === "add"
    ) return create_if_block_72;
    if (
      /*vrMode*/
      ctx2[638] === "edit"
    ) return create_if_block_73;
    return create_else_block_6;
  }
  let current_block_type = select_block_type_11(ctx);
  let if_block1 = current_block_type(ctx);
  let if_block2 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_70(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Virtual Rotator List";
      t1 = space();
      div2 = element("div");
      table = element("table");
      thead = element("thead");
      thead.innerHTML = `<tr><th></th> <th>ID</th> <th>Label</th> <th>Name</th> <th>RX-Only</th> <th>Azimuth Min</th> <th>Azimuth Max</th> <th>Antenna</th></tr>`;
      t16 = space();
      tbody = element("tbody");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t17 = space();
      if (if_block0) if_block0.c();
      t18 = space();
      div1 = element("div");
      if_block1.c();
      t19 = space();
      if (if_block2) if_block2.c();
      attr(div0, "class", "section-title");
      attr(table, "class", "ant-table");
      attr(div1, "class", "button-row");
      set_style(div1, "margin-top", "8px");
      attr(div2, "class", "panel");
      set_style(div2, "overflow-x", "auto");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div2);
      append(div2, table);
      append(table, thead);
      append(table, t16);
      append(table, tbody);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tbody, null);
        }
      }
      append(tbody, t17);
      if (if_block0) if_block0.m(tbody, null);
      append(div2, t18);
      append(div2, div1);
      if_block1.m(div1, null);
      append(div2, t19);
      if (if_block2) if_block2.m(div2, null);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smVrAddAnt, activeSerial, smVrEditForms, smVrMode, smVrSelected*/
      60817409 | dirty[1] & /*activeAllObjs, activeVrs*/
      1310720 | dirty[4] & /*smObjDisplay, smObjRxOnly, smVrToggleSelect, smVrStartAddAnt*/
      3084) {
        each_value_68 = ensure_array_like(
          /*vrs*/
          ctx2[637]
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx2, each_value_68, each_1_lookup, tbody, destroy_block, create_each_block_68, t17, get_each_context_68);
      }
      if (
        /*vrMode*/
        ctx2[638] === "add"
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_74(ctx2);
          if_block0.c();
          if_block0.m(tbody, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (current_block_type === (current_block_type = select_block_type_11(ctx2)) && if_block1) {
        if_block1.p(ctx2, dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type(ctx2);
        if (if_block1) {
          if_block1.c();
          if_block1.m(div1, null);
        }
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_70(ctx2);
          if_block2.c();
          if_block2.m(div2, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d();
      }
      if (if_block0) if_block0.d();
      if_block1.d();
      if (if_block2) if_block2.d();
    }
  };
}
function create_if_block_62(ctx) {
  let section;
  let div0;
  let t1;
  let div2;
  let table;
  let thead;
  let tr;
  let th0;
  let t2;
  let th1;
  let t4;
  let th2;
  let t6;
  let th3;
  let t8;
  let th4;
  let t10;
  let th5;
  let t12;
  let th6;
  let t14;
  let th7;
  let t16;
  let th8;
  let t18;
  let t19;
  let tbody;
  let each_blocks = [];
  let each1_lookup = /* @__PURE__ */ new Map();
  let t20;
  let t21;
  let div1;
  let t22;
  let each_value_67 = ensure_array_like(
    /*antOutCols*/
    ctx[622]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_67.length; i += 1) {
    each_blocks_1[i] = create_each_block_67(get_each_context_67(ctx, each_value_67, i));
  }
  let each_value_64 = ensure_array_like(
    /*ants*/
    ctx[621]
  );
  const get_key = (ctx2) => (
    /*ant*/
    ctx2[627].id
  );
  for (let i = 0; i < each_value_64.length; i += 1) {
    let child_ctx = get_each_context_64(ctx, each_value_64, i);
    let key = get_key(child_ctx);
    each1_lookup.set(key, each_blocks[i] = create_each_block_64(key, child_ctx));
  }
  let if_block0 = (
    /*mode*/
    ctx[547] === "add" && create_if_block_66(get_if_ctx(ctx))
  );
  function select_block_type_6(ctx2, dirty) {
    if (
      /*mode*/
      ctx2[547] === "add"
    ) return create_if_block_64;
    if (
      /*mode*/
      ctx2[547] === "edit"
    ) return create_if_block_65;
    return create_else_block_3;
  }
  let current_block_type = select_block_type_6(ctx);
  let if_block1 = current_block_type(ctx);
  let if_block2 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_63(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Antenna List";
      t1 = space();
      div2 = element("div");
      table = element("table");
      thead = element("thead");
      tr = element("tr");
      th0 = element("th");
      t2 = space();
      th1 = element("th");
      th1.textContent = "ID";
      t4 = space();
      th2 = element("th");
      th2.textContent = "Label";
      t6 = space();
      th3 = element("th");
      th3.textContent = "Name";
      t8 = space();
      th4 = element("th");
      th4.textContent = "SteppIR";
      t10 = space();
      th5 = element("th");
      th5.textContent = "RX-Only";
      t12 = space();
      th6 = element("th");
      th6.textContent = "PA Ant Nr";
      t14 = space();
      th7 = element("th");
      th7.textContent = "Rotator";
      t16 = space();
      th8 = element("th");
      th8.textContent = "Rot. Offset";
      t18 = space();
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t19 = space();
      tbody = element("tbody");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t20 = space();
      if (if_block0) if_block0.c();
      t21 = space();
      div1 = element("div");
      if_block1.c();
      t22 = space();
      if (if_block2) if_block2.c();
      attr(div0, "class", "section-title");
      attr(table, "class", "ant-table");
      attr(div1, "class", "button-row");
      set_style(div1, "margin-top", "8px");
      attr(div2, "class", "panel");
      set_style(div2, "overflow-x", "auto");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div2);
      append(div2, table);
      append(table, thead);
      append(thead, tr);
      append(tr, th0);
      append(tr, t2);
      append(tr, th1);
      append(tr, t4);
      append(tr, th2);
      append(tr, t6);
      append(tr, th3);
      append(tr, t8);
      append(tr, th4);
      append(tr, t10);
      append(tr, th5);
      append(tr, t12);
      append(tr, th6);
      append(tr, t14);
      append(tr, th7);
      append(tr, t16);
      append(tr, th8);
      append(tr, t18);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(tr, null);
        }
      }
      append(table, t19);
      append(table, tbody);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tbody, null);
        }
      }
      append(tbody, t20);
      if (if_block0) if_block0.m(tbody, null);
      append(div2, t21);
      append(div2, div1);
      if_block1.m(div1, null);
      append(div2, t22);
      if (if_block2) if_block2.m(div2, null);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntOutCols*/
      2097152) {
        each_value_67 = ensure_array_like(
          /*antOutCols*/
          ctx2[622]
        );
        let i;
        for (i = 0; i < each_value_67.length; i += 1) {
          const child_ctx = get_each_context_67(ctx2, each_value_67, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_67(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(tr, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_67.length;
      }
      if (dirty[0] & /*smAntEditForms, activeSerial, smAntMode, smAntSelected*/
      1441793 | dirty[1] & /*activeAntOutCols, activeAnts*/
      6291456 | dirty[4] & /*smAntToggleSelect*/
      2) {
        each_value_64 = ensure_array_like(
          /*ants*/
          ctx2[621]
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx2, each_value_64, each1_lookup, tbody, destroy_block, create_each_block_64, t20, get_each_context_64);
      }
      if (
        /*mode*/
        ctx2[547] === "add"
      ) {
        if (if_block0) {
          if_block0.p(get_if_ctx(ctx2), dirty);
        } else {
          if_block0 = create_if_block_66(get_if_ctx(ctx2));
          if_block0.c();
          if_block0.m(tbody, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (current_block_type === (current_block_type = select_block_type_6(ctx2)) && if_block1) {
        if_block1.p(ctx2, dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type(ctx2);
        if (if_block1) {
          if_block1.c();
          if_block1.m(div1, null);
        }
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_63(ctx2);
          if_block2.c();
          if_block2.m(div2, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks_1, detaching);
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d();
      }
      if (if_block0) if_block0.d();
      if_block1.d();
      if (if_block2) if_block2.d();
    }
  };
}
function create_if_block_60(ctx) {
  let section;
  let div0;
  let t1;
  let div1;
  let t2;
  let each_value_61 = ensure_array_like(
    /*smOutputNames*/
    ctx[85]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_61.length; i += 1) {
    each_blocks[i] = create_each_block_61(get_each_context_61(ctx, each_value_61, i));
  }
  let if_block = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_61(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Outputs";
      t1 = space();
      div1 = element("div");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t2 = space();
      if (if_block) if_block.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div1, null);
        }
      }
      append(div1, t2);
      if (if_block) if_block.m(div1, null);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[2] & /*smOutputNames, smOutputClassOptions*/
      12582912 | dirty[3] & /*smOutputVal, applySmOutput*/
      17825792) {
        each_value_61 = ensure_array_like(
          /*smOutputNames*/
          ctx2[85]
        );
        let i;
        for (i = 0; i < each_value_61.length; i += 1) {
          const child_ctx = get_each_context_61(ctx2, each_value_61, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_61(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div1, t2);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_61.length;
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block_61(ctx2);
          if_block.c();
          if_block.m(div1, null);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks, detaching);
      if (if_block) if_block.d();
    }
  };
}
function create_if_block_57(ctx) {
  let section;
  let div0;
  let t1;
  let div31;
  let div3;
  let div1;
  let t3;
  let div2;
  let select0;
  let select0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let select1;
  let select1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let input0;
  let input0_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let select2;
  let select2_value_value;
  let t13;
  let div15;
  let div13;
  let t15;
  let div14;
  let select3;
  let select3_value_value;
  let t16;
  let div18;
  let div16;
  let t18;
  let div17;
  let input1;
  let input1_value_value;
  let t19;
  let div21;
  let div19;
  let t21;
  let div20;
  let input2;
  let input2_value_value;
  let t22;
  let div24;
  let div22;
  let t24;
  let div23;
  let input3;
  let input3_value_value;
  let t25;
  let div27;
  let div25;
  let t27;
  let div26;
  let input4;
  let input4_checked_value;
  let t28;
  let div30;
  let div28;
  let t30;
  let div29;
  let input5;
  let input5_checked_value;
  let t31;
  let t32;
  let show_if = (
    /*smSequencerOutputs*/
    ctx[114](
      /*activeSerial*/
      ctx[0]
    ).length > 0
  );
  let if_block1_anchor;
  let mounted;
  let dispose;
  let each_value_60 = ensure_array_like(
    /*smCivFuncOptions*/
    ctx[81]
  );
  let each_blocks_3 = [];
  for (let i = 0; i < each_value_60.length; i += 1) {
    each_blocks_3[i] = create_each_block_60(get_each_context_60(ctx, each_value_60, i));
  }
  let each_value_59 = ensure_array_like(
    /*smCivBaudOptions*/
    ctx[82]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_59.length; i += 1) {
    each_blocks_2[i] = create_each_block_59(get_each_context_59(ctx, each_value_59, i));
  }
  let each_value_58 = ensure_array_like(
    /*smExtSerFuncOptions*/
    ctx[83]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_58.length; i += 1) {
    each_blocks_1[i] = create_each_block_58(get_each_context_58(ctx, each_value_58, i));
  }
  let each_value_57 = ensure_array_like(
    /*smCivBaudOptions*/
    ctx[82]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_57.length; i += 1) {
    each_blocks[i] = create_each_block_57(get_each_context_57(ctx, each_value_57, i));
  }
  let if_block0 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_59(ctx)
  );
  let if_block1 = show_if && create_if_block_58(ctx);
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Settings";
      t1 = space();
      div31 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "PA connector CI-V Function:";
      t3 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        each_blocks_3[i].c();
      }
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "CI-V Baud Rate:";
      t6 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "CI-V Address:";
      t9 = space();
      div8 = element("div");
      input0 = element("input");
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "D9 Serial Port Function:";
      t12 = space();
      div11 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t13 = space();
      div15 = element("div");
      div13 = element("div");
      div13.textContent = "D9 Serial Port Baud Rate:";
      t15 = space();
      div14 = element("div");
      select3 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t16 = space();
      div18 = element("div");
      div16 = element("div");
      div16.textContent = "Switch Delay (ms):";
      t18 = space();
      div17 = element("div");
      input1 = element("input");
      t19 = space();
      div21 = element("div");
      div19 = element("div");
      div19.textContent = "Break before make delay (ms):";
      t21 = space();
      div20 = element("div");
      input2 = element("input");
      t22 = space();
      div24 = element("div");
      div22 = element("div");
      div22.textContent = "Inhibit lead time (ms):";
      t24 = space();
      div23 = element("div");
      input3 = element("input");
      t25 = space();
      div27 = element("div");
      div25 = element("div");
      div25.textContent = "Use Key In:";
      t27 = space();
      div26 = element("div");
      input4 = element("input");
      t28 = space();
      div30 = element("div");
      div28 = element("div");
      div28.textContent = "Invert Key In:";
      t30 = space();
      div29 = element("div");
      input5 = element("input");
      t31 = space();
      if (if_block0) if_block0.c();
      t32 = space();
      if (if_block1) if_block1.c();
      if_block1_anchor = empty();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      attr(input0, "min", "0");
      attr(input0, "max", "255");
      input0.value = input0_value_value = /*smFixed*/
      ctx[111](
        /*activeSerial*/
        ctx[0],
        "civAddress"
      );
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(select2, "class", "select");
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "label");
      attr(select3, "class", "select");
      attr(div14, "class", "value");
      attr(div15, "class", "row");
      attr(div16, "class", "label");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      attr(input1, "min", "0");
      input1.value = input1_value_value = /*smFixed*/
      ctx[111](
        /*activeSerial*/
        ctx[0],
        "antSwDelay"
      );
      attr(div17, "class", "value");
      attr(div18, "class", "row");
      attr(div19, "class", "label");
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      attr(input2, "min", "0");
      input2.value = input2_value_value = /*smFixed*/
      ctx[111](
        /*activeSerial*/
        ctx[0],
        "bbmDelay"
      );
      attr(div20, "class", "value");
      attr(div21, "class", "row");
      attr(div22, "class", "label");
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      attr(input3, "min", "0");
      input3.value = input3_value_value = /*smFixed*/
      ctx[111](
        /*activeSerial*/
        ctx[0],
        "inhibitLead"
      );
      attr(div23, "class", "value");
      attr(div24, "class", "row");
      attr(div25, "class", "label");
      attr(input4, "type", "checkbox");
      input4.checked = input4_checked_value = /*smFixed*/
      ctx[111](
        /*activeSerial*/
        ctx[0],
        "useKeyIn"
      ) === 1;
      attr(div26, "class", "value");
      attr(div27, "class", "row");
      attr(div28, "class", "label");
      attr(input5, "type", "checkbox");
      input5.checked = input5_checked_value = /*smFixed*/
      ctx[111](
        /*activeSerial*/
        ctx[0],
        "invertKeyIn"
      ) === 1;
      attr(div29, "class", "value");
      attr(div30, "class", "row");
      attr(div31, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div31);
      append(div31, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        if (each_blocks_3[i]) {
          each_blocks_3[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*smFixed*/
        ctx[111](
          /*activeSerial*/
          ctx[0],
          "civFunc"
        )
      );
      append(div31, t4);
      append(div31, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*smFixed*/
        ctx[111](
          /*activeSerial*/
          ctx[0],
          "civBaudRate"
        )
      );
      append(div31, t7);
      append(div31, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, input0);
      append(div31, t10);
      append(div31, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, select2);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*smFixed*/
        ctx[111](
          /*activeSerial*/
          ctx[0],
          "extSerFunc"
        )
      );
      append(div31, t13);
      append(div31, div15);
      append(div15, div13);
      append(div15, t15);
      append(div15, div14);
      append(div14, select3);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select3, null);
        }
      }
      select_option(
        select3,
        /*smFixed*/
        ctx[111](
          /*activeSerial*/
          ctx[0],
          "extSerBaudRate"
        )
      );
      append(div31, t16);
      append(div31, div18);
      append(div18, div16);
      append(div18, t18);
      append(div18, div17);
      append(div17, input1);
      append(div31, t19);
      append(div31, div21);
      append(div21, div19);
      append(div21, t21);
      append(div21, div20);
      append(div20, input2);
      append(div31, t22);
      append(div31, div24);
      append(div24, div22);
      append(div24, t24);
      append(div24, div23);
      append(div23, input3);
      append(div31, t25);
      append(div31, div27);
      append(div27, div25);
      append(div27, t27);
      append(div27, div26);
      append(div26, input4);
      append(div31, t28);
      append(div31, div30);
      append(div30, div28);
      append(div30, t30);
      append(div30, div29);
      append(div29, input5);
      append(div31, t31);
      if (if_block0) if_block0.m(div31, null);
      insert(target, t32, anchor);
      if (if_block1) if_block1.m(target, anchor);
      insert(target, if_block1_anchor, anchor);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_107*/
            ctx[328]
          ),
          listen(
            select1,
            "change",
            /*change_handler_108*/
            ctx[329]
          ),
          listen(
            input0,
            "change",
            /*change_handler_109*/
            ctx[330]
          ),
          listen(
            select2,
            "change",
            /*change_handler_110*/
            ctx[331]
          ),
          listen(
            select3,
            "change",
            /*change_handler_111*/
            ctx[332]
          ),
          listen(
            input1,
            "change",
            /*change_handler_112*/
            ctx[333]
          ),
          listen(
            input2,
            "change",
            /*change_handler_113*/
            ctx[334]
          ),
          listen(
            input3,
            "change",
            /*change_handler_114*/
            ctx[335]
          ),
          listen(
            input4,
            "change",
            /*change_handler_115*/
            ctx[336]
          ),
          listen(
            input5,
            "change",
            /*change_handler_116*/
            ctx[337]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[2] & /*smCivFuncOptions*/
      524288) {
        each_value_60 = ensure_array_like(
          /*smCivFuncOptions*/
          ctx2[81]
        );
        let i;
        for (i = 0; i < each_value_60.length; i += 1) {
          const child_ctx = get_each_context_60(ctx2, each_value_60, i);
          if (each_blocks_3[i]) {
            each_blocks_3[i].p(child_ctx, dirty);
          } else {
            each_blocks_3[i] = create_each_block_60(child_ctx);
            each_blocks_3[i].c();
            each_blocks_3[i].m(select0, null);
          }
        }
        for (; i < each_blocks_3.length; i += 1) {
          each_blocks_3[i].d(1);
        }
        each_blocks_3.length = each_value_60.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "civFunc"
      ))) {
        select_option(
          select0,
          /*smFixed*/
          ctx2[111](
            /*activeSerial*/
            ctx2[0],
            "civFunc"
          )
        );
      }
      if (dirty[2] & /*smCivBaudOptions*/
      1048576) {
        each_value_59 = ensure_array_like(
          /*smCivBaudOptions*/
          ctx2[82]
        );
        let i;
        for (i = 0; i < each_value_59.length; i += 1) {
          const child_ctx = get_each_context_59(ctx2, each_value_59, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_59(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select1, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_59.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "civBaudRate"
      ))) {
        select_option(
          select1,
          /*smFixed*/
          ctx2[111](
            /*activeSerial*/
            ctx2[0],
            "civBaudRate"
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "civAddress"
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[2] & /*smExtSerFuncOptions*/
      2097152) {
        each_value_58 = ensure_array_like(
          /*smExtSerFuncOptions*/
          ctx2[83]
        );
        let i;
        for (i = 0; i < each_value_58.length; i += 1) {
          const child_ctx = get_each_context_58(ctx2, each_value_58, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_58(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select2, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_58.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select2_value_value !== (select2_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "extSerFunc"
      ))) {
        select_option(
          select2,
          /*smFixed*/
          ctx2[111](
            /*activeSerial*/
            ctx2[0],
            "extSerFunc"
          )
        );
      }
      if (dirty[2] & /*smCivBaudOptions*/
      1048576) {
        each_value_57 = ensure_array_like(
          /*smCivBaudOptions*/
          ctx2[82]
        );
        let i;
        for (i = 0; i < each_value_57.length; i += 1) {
          const child_ctx = get_each_context_57(ctx2, each_value_57, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_57(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select3, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_57.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select3_value_value !== (select3_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "extSerBaudRate"
      ))) {
        select_option(
          select3,
          /*smFixed*/
          ctx2[111](
            /*activeSerial*/
            ctx2[0],
            "extSerBaudRate"
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_value_value !== (input1_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "antSwDelay"
      )) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_value_value !== (input2_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "bbmDelay"
      )) && input2.value !== input2_value_value) {
        input2.value = input2_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input3_value_value !== (input3_value_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "inhibitLead"
      )) && input3.value !== input3_value_value) {
        input3.value = input3_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input4_checked_value !== (input4_checked_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "useKeyIn"
      ) === 1)) {
        input4.checked = input4_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input5_checked_value !== (input5_checked_value = /*smFixed*/
      ctx2[111](
        /*activeSerial*/
        ctx2[0],
        "invertKeyIn"
      ) === 1)) {
        input5.checked = input5_checked_value;
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_59(ctx2);
          if_block0.c();
          if_block0.m(div31, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[0] & /*activeSerial*/
      1) show_if = /*smSequencerOutputs*/
      ctx2[114](
        /*activeSerial*/
        ctx2[0]
      ).length > 0;
      if (show_if) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_58(ctx2);
          if_block1.c();
          if_block1.m(if_block1_anchor.parentNode, if_block1_anchor);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
        detach(t32);
        detach(if_block1_anchor);
      }
      destroy_each(each_blocks_3, detaching);
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d(detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_55(ctx) {
  let section;
  let div0;
  let t1;
  let div7;
  let div3;
  let div1;
  let button0;
  let t2;
  let button0_disabled_value;
  let t3;
  let div2;
  let t5;
  let div6;
  let div4;
  let button1;
  let t6;
  let button1_disabled_value;
  let t7;
  let div5;
  let t9;
  let mounted;
  let dispose;
  let if_block = (
    /*smActionStatus*/
    ctx[15][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_56(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Load & Store Antenna Switching Settings";
      t1 = space();
      div7 = element("div");
      div3 = element("div");
      div1 = element("div");
      button0 = element("button");
      t2 = text("Load");
      t3 = space();
      div2 = element("div");
      div2.textContent = "Retrieve antenna switching settings from Station Master";
      t5 = space();
      div6 = element("div");
      div4 = element("div");
      button1 = element("button");
      t6 = text("Store");
      t7 = space();
      div5 = element("div");
      div5.textContent = "Send antenna switching settings to Station Master";
      t9 = space();
      if (if_block) if_block.c();
      attr(div0, "class", "section-title");
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      button0.disabled = button0_disabled_value = /*smActionBusy*/
      ctx[16][
        /*activeSerial*/
        ctx[0]
      ];
      attr(div1, "class", "label");
      set_style(div1, "text-align", "left");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
      button1.disabled = button1_disabled_value = /*smActionBusy*/
      ctx[16][
        /*activeSerial*/
        ctx[0]
      ];
      attr(div4, "class", "label");
      set_style(div4, "text-align", "left");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div7);
      append(div7, div3);
      append(div3, div1);
      append(div1, button0);
      append(button0, t2);
      append(div3, t3);
      append(div3, div2);
      append(div7, t5);
      append(div7, div6);
      append(div6, div4);
      append(div4, button1);
      append(button1, t6);
      append(div6, t7);
      append(div6, div5);
      append(div7, t9);
      if (if_block) if_block.m(div7, null);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_10*/
            ctx[326]
          ),
          listen(
            button1,
            "click",
            /*click_handler_11*/
            ctx[327]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smActionBusy, activeSerial*/
      65537 | dirty[1] & /*activeAntsAndGroups*/
      8192 && button0_disabled_value !== (button0_disabled_value = /*smActionBusy*/
      ctx2[16][
        /*activeSerial*/
        ctx2[0]
      ])) {
        button0.disabled = button0_disabled_value;
      }
      if (dirty[0] & /*smActionBusy, activeSerial*/
      65537 | dirty[1] & /*activeAntsAndGroups*/
      8192 && button1_disabled_value !== (button1_disabled_value = /*smActionBusy*/
      ctx2[16][
        /*activeSerial*/
        ctx2[0]
      ])) {
        button1.disabled = button1_disabled_value;
      }
      if (
        /*smActionStatus*/
        ctx2[15][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block_56(ctx2);
          if_block.c();
          if_block.m(div7, null);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      if (if_block) if_block.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_52(ctx) {
  let section0;
  let div0;
  let t1;
  let div2;
  let t2;
  let div1;
  let button0;
  let t4;
  let t5;
  let section1;
  let div3;
  let t7;
  let div5;
  let t8;
  let div4;
  let button1;
  let t10;
  let mounted;
  let dispose;
  let each_value_55 = ensure_array_like(
    /*messageSlots*/
    ctx[78]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_55.length; i += 1) {
    each_blocks_1[i] = create_each_block_55(get_each_context_55(ctx, each_value_55, i));
  }
  let if_block0 = (
    /*messageStatus*/
    ctx[12][`${/*activeSerial*/
    ctx[0]}:cw`] && create_if_block_54(ctx)
  );
  let each_value_54 = ensure_array_like(
    /*messageSlots*/
    ctx[78]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_54.length; i += 1) {
    each_blocks[i] = create_each_block_54(get_each_context_54(ctx, each_value_54, i));
  }
  let if_block1 = (
    /*messageStatus*/
    ctx[12][`${/*activeSerial*/
    ctx[0]}:fsk`] && create_if_block_53(ctx)
  );
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "CW Messages";
      t1 = space();
      div2 = element("div");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t2 = space();
      div1 = element("div");
      button0 = element("button");
      button0.textContent = "Apply";
      t4 = space();
      if (if_block0) if_block0.c();
      t5 = space();
      section1 = element("section");
      div3 = element("div");
      div3.textContent = "FSK Messages";
      t7 = space();
      div5 = element("div");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t8 = space();
      div4 = element("div");
      button1 = element("button");
      button1.textContent = "Apply";
      t10 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(div1, "class", "button-row");
      attr(div2, "class", "panel");
      attr(section0, "class", "section");
      attr(div3, "class", "section-title");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
      attr(div4, "class", "button-row");
      attr(div5, "class", "panel");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div2);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(div2, null);
        }
      }
      append(div2, t2);
      append(div2, div1);
      append(div1, button0);
      append(div2, t4);
      if (if_block0) if_block0.m(div2, null);
      insert(target, t5, anchor);
      insert(target, section1, anchor);
      append(section1, div3);
      append(section1, t7);
      append(section1, div5);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div5, null);
        }
      }
      append(div5, t8);
      append(div5, div4);
      append(div4, button1);
      append(div5, t10);
      if (if_block1) if_block1.m(div5, null);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_7*/
            ctx[322]
          ),
          listen(
            button1,
            "click",
            /*click_handler_9*/
            ctx[325]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial, messageForm*/
      2049 | dirty[2] & /*playMessage, messageSlots, handleMessageInput*/
      536936452) {
        each_value_55 = ensure_array_like(
          /*messageSlots*/
          ctx2[78]
        );
        let i;
        for (i = 0; i < each_value_55.length; i += 1) {
          const child_ctx = get_each_context_55(ctx2, each_value_55, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_55(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(div2, t2);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_55.length;
      }
      if (
        /*messageStatus*/
        ctx2[12][`${/*activeSerial*/
        ctx2[0]}:cw`]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_54(ctx2);
          if_block0.c();
          if_block0.m(div2, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[0] & /*activeSerial, messageForm*/
      2049 | dirty[2] & /*playMessage, messageSlots, handleMessageInput*/
      536936452) {
        each_value_54 = ensure_array_like(
          /*messageSlots*/
          ctx2[78]
        );
        let i;
        for (i = 0; i < each_value_54.length; i += 1) {
          const child_ctx = get_each_context_54(ctx2, each_value_54, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_54(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div5, t8);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_54.length;
      }
      if (
        /*messageStatus*/
        ctx2[12][`${/*activeSerial*/
        ctx2[0]}:fsk`]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_53(ctx2);
          if_block1.c();
          if_block1.m(div5, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t5);
        detach(section1);
      }
      destroy_each(each_blocks_1, detaching);
      if (if_block0) if_block0.d();
      destroy_each(each_blocks, detaching);
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_49(ctx) {
  var _a, _b, _c, _d;
  let section0;
  let div0;
  let t1;
  let div52;
  let t2;
  let div3;
  let div1;
  let t4;
  let div2;
  let select0;
  let select0_value_value;
  let t5;
  let div6;
  let div4;
  let t7;
  let div5;
  let select1;
  let select1_value_value;
  let t8;
  let div9;
  let div7;
  let t10;
  let div8;
  let input0;
  let input0_value_value;
  let t11;
  let div12;
  let div10;
  let t13;
  let div11;
  let input1;
  let input1_value_value;
  let t14;
  let div15;
  let div13;
  let t16;
  let div14;
  let input2;
  let input2_value_value;
  let t17;
  let div18;
  let div16;
  let t19;
  let div17;
  let input3;
  let input3_checked_value;
  let t20;
  let div21;
  let div19;
  let t22;
  let div20;
  let input4;
  let input4_checked_value;
  let t23;
  let div24;
  let div22;
  let t25;
  let div23;
  let input5;
  let input5_checked_value;
  let t26;
  let div27;
  let div25;
  let t28;
  let div26;
  let input6;
  let input6_checked_value;
  let t29;
  let div30;
  let div28;
  let t31;
  let div29;
  let input7;
  let input7_value_value;
  let t32;
  let div33;
  let div31;
  let t34;
  let div32;
  let input8;
  let input8_value_value;
  let t35;
  let div36;
  let div34;
  let t37;
  let div35;
  let input9;
  let input9_value_value;
  let t38;
  let div39;
  let div37;
  let t40;
  let div38;
  let input10;
  let input10_value_value;
  let t41;
  let div42;
  let div40;
  let t43;
  let div41;
  let input11;
  let input11_value_value;
  let t44;
  let div45;
  let div43;
  let t46;
  let div44;
  let input12;
  let input12_value_value;
  let t47;
  let div48;
  let div46;
  let t49;
  let div47;
  let input13;
  let input13_value_value;
  let t50;
  let div51;
  let div49;
  let t52;
  let div50;
  let input14;
  let input14_value_value;
  let t53;
  let section1;
  let div53;
  let t55;
  let div57;
  let div56;
  let div54;
  let t57;
  let div55;
  let select2;
  let select2_value_value;
  let t58;
  let mounted;
  let dispose;
  let if_block0 = (
    /*activeKeyer*/
    (((_a = ctx[2]) == null ? void 0 : _a.type) === 3 || /*activeKeyer*/
    ((_b = ctx[2]) == null ? void 0 : _b.type) === 5) && create_if_block_51(ctx)
  );
  let each_value_53 = ensure_array_like(
    /*winkeyModeOptions*/
    ctx[74]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_53.length; i += 1) {
    each_blocks_2[i] = create_each_block_53(get_each_context_53(ctx, each_value_53, i));
  }
  let each_value_52 = ensure_array_like(
    /*winkeyUltimaticOptions*/
    ctx[75]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_52.length; i += 1) {
    each_blocks_1[i] = create_each_block_52(get_each_context_52(ctx, each_value_52, i));
  }
  let each_value_51 = ensure_array_like(
    /*sideToneOptions*/
    ctx[76]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_51.length; i += 1) {
    each_blocks[i] = create_each_block_51(get_each_context_51(ctx, each_value_51, i));
  }
  let if_block1 = (
    /*activeKeyer*/
    (((_c = ctx[2]) == null ? void 0 : _c.type) === 3 || /*activeKeyer*/
    ((_d = ctx[2]) == null ? void 0 : _d.type) === 5) && create_if_block_50(ctx)
  );
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "Winkey";
      t1 = space();
      div52 = element("div");
      if (if_block0) if_block0.c();
      t2 = space();
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Paddle Mode:";
      t4 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t5 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Ultimatic Mode:";
      t7 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t8 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "PTT Lead (x 10ms):";
      t10 = space();
      div8 = element("div");
      input0 = element("input");
      t11 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "PTT Tail (x 10ms):";
      t13 = space();
      div11 = element("div");
      input1 = element("input");
      t14 = space();
      div15 = element("div");
      div13 = element("div");
      div13.textContent = "Hang Time:";
      t16 = space();
      div14 = element("div");
      input2 = element("input");
      t17 = space();
      div18 = element("div");
      div16 = element("div");
      div16.textContent = "Swap Paddles:";
      t19 = space();
      div17 = element("div");
      input3 = element("input");
      t20 = space();
      div21 = element("div");
      div19 = element("div");
      div19.textContent = "Auto Space:";
      t22 = space();
      div20 = element("div");
      input4 = element("input");
      t23 = space();
      div24 = element("div");
      div22 = element("div");
      div22.textContent = "CT Spacing:";
      t25 = space();
      div23 = element("div");
      input5 = element("input");
      t26 = space();
      div27 = element("div");
      div25 = element("div");
      div25.textContent = "Disable Paddle Watchdog:";
      t28 = space();
      div26 = element("div");
      input6 = element("input");
      t29 = space();
      div30 = element("div");
      div28 = element("div");
      div28.textContent = "Speed Pot Min WPM:";
      t31 = space();
      div29 = element("div");
      input7 = element("input");
      t32 = space();
      div33 = element("div");
      div31 = element("div");
      div31.textContent = "Speed Pot Range WPM:";
      t34 = space();
      div32 = element("div");
      input8 = element("input");
      t35 = space();
      div36 = element("div");
      div34 = element("div");
      div34.textContent = "Farnsworth Speed WPM:";
      t37 = space();
      div35 = element("div");
      input9 = element("input");
      t38 = space();
      div39 = element("div");
      div37 = element("div");
      div37.textContent = "DAH/DIT = 3*(nn/50):";
      t40 = space();
      div38 = element("div");
      input10 = element("input");
      t41 = space();
      div42 = element("div");
      div40 = element("div");
      div40.textContent = "Weighting:";
      t43 = space();
      div41 = element("div");
      input11 = element("input");
      t44 = space();
      div45 = element("div");
      div43 = element("div");
      div43.textContent = "1st Extension:";
      t46 = space();
      div44 = element("div");
      input12 = element("input");
      t47 = space();
      div48 = element("div");
      div46 = element("div");
      div46.textContent = "Keying Compensation:";
      t49 = space();
      div47 = element("div");
      input13 = element("input");
      t50 = space();
      div51 = element("div");
      div49 = element("div");
      div49.textContent = "Paddle Setpoint:";
      t52 = space();
      div50 = element("div");
      input14 = element("input");
      t53 = space();
      section1 = element("section");
      div53 = element("div");
      div53.textContent = "Side Tone";
      t55 = space();
      div57 = element("div");
      div56 = element("div");
      div54 = element("div");
      div54.textContent = "Side Tone:";
      t57 = space();
      div55 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t58 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "leadInTime"
      );
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      input1.value = input1_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "tailTime"
      );
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "label");
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      input2.value = input2_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "hangTime"
      );
      attr(div14, "class", "value");
      attr(div15, "class", "row");
      attr(div16, "class", "label");
      attr(input3, "type", "checkbox");
      input3.checked = input3_checked_value = !!/*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "paddleSwap"
      );
      attr(div17, "class", "value");
      attr(div18, "class", "row");
      attr(div19, "class", "label");
      attr(input4, "type", "checkbox");
      input4.checked = input4_checked_value = !!/*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "autoSpace"
      );
      attr(div20, "class", "value");
      attr(div21, "class", "row");
      attr(div22, "class", "label");
      attr(input5, "type", "checkbox");
      input5.checked = input5_checked_value = !!/*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "ctSpacing"
      );
      attr(div23, "class", "value");
      attr(div24, "class", "row");
      attr(div25, "class", "label");
      attr(input6, "type", "checkbox");
      input6.checked = input6_checked_value = !!/*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "disablePaddleWatchdog"
      );
      attr(div26, "class", "value");
      attr(div27, "class", "row");
      attr(div28, "class", "label");
      attr(input7, "class", "input");
      attr(input7, "type", "number");
      input7.value = input7_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "minWpm"
      );
      attr(div29, "class", "value");
      attr(div30, "class", "row");
      attr(div31, "class", "label");
      attr(input8, "class", "input");
      attr(input8, "type", "number");
      input8.value = input8_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "wpmRange"
      );
      attr(div32, "class", "value");
      attr(div33, "class", "row");
      attr(div34, "class", "label");
      attr(input9, "class", "input");
      attr(input9, "type", "number");
      input9.value = input9_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "farnsWpm"
      );
      attr(div35, "class", "value");
      attr(div36, "class", "row");
      attr(div37, "class", "label");
      attr(input10, "class", "input");
      attr(input10, "type", "number");
      input10.value = input10_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "ditDahRatio"
      );
      attr(div38, "class", "value");
      attr(div39, "class", "row");
      attr(div40, "class", "label");
      attr(input11, "class", "input");
      attr(input11, "type", "number");
      input11.value = input11_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "weight"
      );
      attr(div41, "class", "value");
      attr(div42, "class", "row");
      attr(div43, "class", "label");
      attr(input12, "class", "input");
      attr(input12, "type", "number");
      input12.value = input12_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "1stExtension"
      );
      attr(div44, "class", "value");
      attr(div45, "class", "row");
      attr(div46, "class", "label");
      attr(input13, "class", "input");
      attr(input13, "type", "number");
      input13.value = input13_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "keyComp"
      );
      attr(div47, "class", "value");
      attr(div48, "class", "row");
      attr(div49, "class", "label");
      attr(input14, "class", "input");
      attr(input14, "type", "number");
      input14.value = input14_value_value = /*winkeyValue*/
      ctx[108](
        /*activeSerial*/
        ctx[0],
        "paddleSetpoint"
      );
      attr(div50, "class", "value");
      attr(div51, "class", "row");
      attr(div52, "class", "panel");
      attr(section0, "class", "section");
      attr(div53, "class", "section-title");
      attr(div54, "class", "label");
      attr(select2, "class", "select");
      attr(div55, "class", "value");
      attr(div56, "class", "row");
      attr(div57, "class", "panel");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div52);
      if (if_block0) if_block0.m(div52, null);
      append(div52, t2);
      append(div52, div3);
      append(div3, div1);
      append(div3, t4);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*winkeyValue*/
        ctx[108](
          /*activeSerial*/
          ctx[0],
          "keyMode"
        )
      );
      append(div52, t5);
      append(div52, div6);
      append(div6, div4);
      append(div6, t7);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*winkeyValue*/
        ctx[108](
          /*activeSerial*/
          ctx[0],
          "ultimaticMode"
        )
      );
      append(div52, t8);
      append(div52, div9);
      append(div9, div7);
      append(div9, t10);
      append(div9, div8);
      append(div8, input0);
      append(div52, t11);
      append(div52, div12);
      append(div12, div10);
      append(div12, t13);
      append(div12, div11);
      append(div11, input1);
      append(div52, t14);
      append(div52, div15);
      append(div15, div13);
      append(div15, t16);
      append(div15, div14);
      append(div14, input2);
      append(div52, t17);
      append(div52, div18);
      append(div18, div16);
      append(div18, t19);
      append(div18, div17);
      append(div17, input3);
      append(div52, t20);
      append(div52, div21);
      append(div21, div19);
      append(div21, t22);
      append(div21, div20);
      append(div20, input4);
      append(div52, t23);
      append(div52, div24);
      append(div24, div22);
      append(div24, t25);
      append(div24, div23);
      append(div23, input5);
      append(div52, t26);
      append(div52, div27);
      append(div27, div25);
      append(div27, t28);
      append(div27, div26);
      append(div26, input6);
      append(div52, t29);
      append(div52, div30);
      append(div30, div28);
      append(div30, t31);
      append(div30, div29);
      append(div29, input7);
      append(div52, t32);
      append(div52, div33);
      append(div33, div31);
      append(div33, t34);
      append(div33, div32);
      append(div32, input8);
      append(div52, t35);
      append(div52, div36);
      append(div36, div34);
      append(div36, t37);
      append(div36, div35);
      append(div35, input9);
      append(div52, t38);
      append(div52, div39);
      append(div39, div37);
      append(div39, t40);
      append(div39, div38);
      append(div38, input10);
      append(div52, t41);
      append(div52, div42);
      append(div42, div40);
      append(div42, t43);
      append(div42, div41);
      append(div41, input11);
      append(div52, t44);
      append(div52, div45);
      append(div45, div43);
      append(div45, t46);
      append(div45, div44);
      append(div44, input12);
      append(div52, t47);
      append(div52, div48);
      append(div48, div46);
      append(div48, t49);
      append(div48, div47);
      append(div47, input13);
      append(div52, t50);
      append(div52, div51);
      append(div51, div49);
      append(div51, t52);
      append(div51, div50);
      append(div50, input14);
      insert(target, t53, anchor);
      insert(target, section1, anchor);
      append(section1, div53);
      append(section1, t55);
      append(section1, div57);
      append(div57, div56);
      append(div56, div54);
      append(div56, t57);
      append(div56, div55);
      append(div55, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          "sideTone"
        )
      );
      append(div57, t58);
      if (if_block1) if_block1.m(div57, null);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_99*/
            ctx[301]
          ),
          listen(
            select1,
            "change",
            /*change_handler_100*/
            ctx[302]
          ),
          listen(
            input0,
            "input",
            /*input_handler_24*/
            ctx[303]
          ),
          listen(
            input1,
            "input",
            /*input_handler_25*/
            ctx[304]
          ),
          listen(
            input2,
            "input",
            /*input_handler_26*/
            ctx[305]
          ),
          listen(
            input3,
            "change",
            /*change_handler_101*/
            ctx[306]
          ),
          listen(
            input4,
            "change",
            /*change_handler_102*/
            ctx[307]
          ),
          listen(
            input5,
            "change",
            /*change_handler_103*/
            ctx[308]
          ),
          listen(
            input6,
            "change",
            /*change_handler_104*/
            ctx[309]
          ),
          listen(
            input7,
            "input",
            /*input_handler_27*/
            ctx[310]
          ),
          listen(
            input8,
            "input",
            /*input_handler_28*/
            ctx[311]
          ),
          listen(
            input9,
            "input",
            /*input_handler_29*/
            ctx[312]
          ),
          listen(
            input10,
            "input",
            /*input_handler_30*/
            ctx[313]
          ),
          listen(
            input11,
            "input",
            /*input_handler_31*/
            ctx[314]
          ),
          listen(
            input12,
            "input",
            /*input_handler_32*/
            ctx[315]
          ),
          listen(
            input13,
            "input",
            /*input_handler_33*/
            ctx[316]
          ),
          listen(
            input14,
            "input",
            /*input_handler_34*/
            ctx[317]
          ),
          listen(
            select2,
            "change",
            /*change_handler_105*/
            ctx[318]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c2, _d2;
      if (
        /*activeKeyer*/
        ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) === 3 || /*activeKeyer*/
        ((_b2 = ctx2[2]) == null ? void 0 : _b2.type) === 5
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_51(ctx2);
          if_block0.c();
          if_block0.m(div52, t2);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[2] & /*winkeyModeOptions*/
      4096) {
        each_value_53 = ensure_array_like(
          /*winkeyModeOptions*/
          ctx2[74]
        );
        let i;
        for (i = 0; i < each_value_53.length; i += 1) {
          const child_ctx = get_each_context_53(ctx2, each_value_53, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_53(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_53.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "keyMode"
      ))) {
        select_option(
          select0,
          /*winkeyValue*/
          ctx2[108](
            /*activeSerial*/
            ctx2[0],
            "keyMode"
          )
        );
      }
      if (dirty[2] & /*winkeyUltimaticOptions*/
      8192) {
        each_value_52 = ensure_array_like(
          /*winkeyUltimaticOptions*/
          ctx2[75]
        );
        let i;
        for (i = 0; i < each_value_52.length; i += 1) {
          const child_ctx = get_each_context_52(ctx2, each_value_52, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_52(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_52.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "ultimaticMode"
      ))) {
        select_option(
          select1,
          /*winkeyValue*/
          ctx2[108](
            /*activeSerial*/
            ctx2[0],
            "ultimaticMode"
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "leadInTime"
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_value_value !== (input1_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "tailTime"
      )) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_value_value !== (input2_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "hangTime"
      )) && input2.value !== input2_value_value) {
        input2.value = input2_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input3_checked_value !== (input3_checked_value = !!/*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "paddleSwap"
      ))) {
        input3.checked = input3_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input4_checked_value !== (input4_checked_value = !!/*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "autoSpace"
      ))) {
        input4.checked = input4_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input5_checked_value !== (input5_checked_value = !!/*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "ctSpacing"
      ))) {
        input5.checked = input5_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input6_checked_value !== (input6_checked_value = !!/*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "disablePaddleWatchdog"
      ))) {
        input6.checked = input6_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input7_value_value !== (input7_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "minWpm"
      )) && input7.value !== input7_value_value) {
        input7.value = input7_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input8_value_value !== (input8_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "wpmRange"
      )) && input8.value !== input8_value_value) {
        input8.value = input8_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input9_value_value !== (input9_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "farnsWpm"
      )) && input9.value !== input9_value_value) {
        input9.value = input9_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input10_value_value !== (input10_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "ditDahRatio"
      )) && input10.value !== input10_value_value) {
        input10.value = input10_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input11_value_value !== (input11_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "weight"
      )) && input11.value !== input11_value_value) {
        input11.value = input11_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input12_value_value !== (input12_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "1stExtension"
      )) && input12.value !== input12_value_value) {
        input12.value = input12_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input13_value_value !== (input13_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "keyComp"
      )) && input13.value !== input13_value_value) {
        input13.value = input13_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input14_value_value !== (input14_value_value = /*winkeyValue*/
      ctx2[108](
        /*activeSerial*/
        ctx2[0],
        "paddleSetpoint"
      )) && input14.value !== input14_value_value) {
        input14.value = input14_value_value;
      }
      if (dirty[2] & /*sideToneOptions*/
      16384) {
        each_value_51 = ensure_array_like(
          /*sideToneOptions*/
          ctx2[76]
        );
        let i;
        for (i = 0; i < each_value_51.length; i += 1) {
          const child_ctx = get_each_context_51(ctx2, each_value_51, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_51(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_51.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select2_value_value !== (select2_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "sideTone"
      ))) {
        select_option(
          select2,
          /*keyerParam*/
          ctx2[105](
            /*activeSerial*/
            ctx2[0],
            "sideTone"
          )
        );
      }
      if (
        /*activeKeyer*/
        ((_c2 = ctx2[2]) == null ? void 0 : _c2.type) === 3 || /*activeKeyer*/
        ((_d2 = ctx2[2]) == null ? void 0 : _d2.type) === 5
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_50(ctx2);
          if_block1.c();
          if_block1.m(div57, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t53);
        detach(section1);
      }
      if (if_block0) if_block0.d();
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_48(ctx) {
  var _a, _b, _c;
  let section0;
  let div0;
  let t1;
  let div13;
  let div3;
  let div1;
  let t3;
  let div2;
  let input0;
  let input0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let input1;
  let input1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let select0;
  let select0_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let select1;
  let select1_value_value;
  let t13;
  let section1;
  let div14;
  let t15;
  let div19;
  let div18;
  let t21;
  let mounted;
  let dispose;
  let each_value_50 = ensure_array_like(
    /*displayBackgroundOptions*/
    ctx[98](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0
    )
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_50.length; i += 1) {
    each_blocks_2[i] = create_each_block_50(get_each_context_50(ctx, each_value_50, i));
  }
  let each_value_49 = ensure_array_like(
    /*displayBackgroundOptions*/
    ctx[98](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0
    )
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_49.length; i += 1) {
    each_blocks_1[i] = create_each_block_49(get_each_context_49(ctx, each_value_49, i));
  }
  let each_value_47 = ensure_array_like(
    /*displayEventOptions*/
    ctx[99](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_47.length; i += 1) {
    each_blocks[i] = create_each_block_47(get_each_context_47(ctx, each_value_47, i));
  }
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "Display Options";
      t1 = space();
      div13 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Light (0-25):";
      t3 = space();
      div2 = element("div");
      input0 = element("input");
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Contrast (0-25):";
      t6 = space();
      div5 = element("div");
      input1 = element("input");
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Background Upper Line:";
      t9 = space();
      div8 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "Background Lower Line:";
      t12 = space();
      div11 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t13 = space();
      section1 = element("section");
      div14 = element("div");
      div14.textContent = "Display Reports";
      t15 = space();
      div19 = element("div");
      div18 = element("div");
      div18.innerHTML = `<div>Event</div> <div>Enabled</div> <div>Line</div>`;
      t21 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "pwmBlight"
      );
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      input1.value = input1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "pwmContrast"
      );
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(select0, "class", "select");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(select1, "class", "select");
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "panel");
      attr(section0, "class", "section");
      attr(div14, "class", "section-title");
      attr(div18, "class", "table-header");
      attr(div19, "class", "panel table display-grid");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div13);
      append(div13, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, input0);
      append(div13, t4);
      append(div13, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, input1);
      append(div13, t7);
      append(div13, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          "dispBg0"
        )
      );
      append(div13, t10);
      append(div13, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          "dispBg1"
        )
      );
      insert(target, t13, anchor);
      insert(target, section1, anchor);
      append(section1, div14);
      append(section1, t15);
      append(section1, div19);
      append(div19, div18);
      append(div19, t21);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div19, null);
        }
      }
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input_handler_22*/
            ctx[294]
          ),
          listen(
            input1,
            "input",
            /*input_handler_23*/
            ctx[295]
          ),
          listen(
            select0,
            "change",
            /*change_handler_94*/
            ctx[296]
          ),
          listen(
            select1,
            "change",
            /*change_handler_95*/
            ctx[297]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c2;
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "pwmBlight"
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_value_value !== (input1_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "pwmContrast"
      )) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*displayBackgroundOptions*/
      32) {
        each_value_50 = ensure_array_like(
          /*displayBackgroundOptions*/
          ctx2[98](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0
          )
        );
        let i;
        for (i = 0; i < each_value_50.length; i += 1) {
          const child_ctx = get_each_context_50(ctx2, each_value_50, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_50(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_50.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "dispBg0"
      ))) {
        select_option(
          select0,
          /*keyerParam*/
          ctx2[105](
            /*activeSerial*/
            ctx2[0],
            "dispBg0"
          )
        );
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*displayBackgroundOptions*/
      32) {
        each_value_49 = ensure_array_like(
          /*displayBackgroundOptions*/
          ctx2[98](
            /*activeKeyer*/
            ((_b2 = ctx2[2]) == null ? void 0 : _b2.type) ?? 0
          )
        );
        let i;
        for (i = 0; i < each_value_49.length; i += 1) {
          const child_ctx = get_each_context_49(ctx2, each_value_49, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_49(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_49.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "dispBg1"
      ))) {
        select_option(
          select1,
          /*keyerParam*/
          ctx2[105](
            /*activeSerial*/
            ctx2[0],
            "dispBg1"
          )
        );
      }
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[2] & /*displayLineOptions*/
      32768 | dirty[3] & /*keyerParam, displayEventOptions, updateKeyerParam*/
      12352) {
        each_value_47 = ensure_array_like(
          /*displayEventOptions*/
          ctx2[99](
            /*activeKeyer*/
            ((_c2 = ctx2[2]) == null ? void 0 : _c2.type) ?? 0
          )
        );
        let i;
        for (i = 0; i < each_value_47.length; i += 1) {
          const child_ctx = get_each_context_47(ctx2, each_value_47, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_47(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div19, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_47.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t13);
        detach(section1);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_44(ctx) {
  let if_block_anchor;
  function select_block_type_3(ctx2, dirty) {
    if (
      /*isAudioMk2*/
      ctx2[55]
    ) return create_if_block_45;
    if (
      /*isAudioMk1*/
      ctx2[56]
    ) return create_if_block_46;
    if (
      /*isAudioMk2R*/
      ctx2[54]
    ) return create_if_block_47;
    return create_else_block_2;
  }
  let current_block_type = select_block_type_3(ctx);
  let if_block = current_block_type(ctx);
  return {
    c() {
      if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type_3(ctx2)) && if_block) {
        if_block.p(ctx2, dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(ctx2);
        if (if_block) {
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if_block.d(detaching);
    }
  };
}
function create_if_block_29(ctx) {
  var _a, _b, _c;
  let section0;
  let div0;
  let t1;
  let div7;
  let div3;
  let div1;
  let t3;
  let div2;
  let select;
  let select_value_value;
  let t4;
  let show_if_1 = (
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      "voice"
    ).length
  );
  let t5;
  let show_if = (
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0,
      "digital"
    ).length
  );
  let t6;
  let t7;
  let t8;
  let div6;
  let div4;
  let t10;
  let div5;
  let input0;
  let input0_value_value;
  let t11;
  let t12;
  let t13;
  let t14;
  let t15;
  let section1;
  let div8;
  let t17;
  let div24;
  let div11;
  let div9;
  let t19;
  let div10;
  let input1;
  let input1_checked_value;
  let t20;
  let div14;
  let div12;
  let t22;
  let div13;
  let input2;
  let input2_checked_value;
  let t23;
  let div17;
  let div15;
  let t25;
  let div16;
  let input3;
  let input3_checked_value;
  let t26;
  let div20;
  let div18;
  let t28;
  let div19;
  let input4;
  let input4_checked_value;
  let t29;
  let div23;
  let div21;
  let t31;
  let div22;
  let input5;
  let input5_checked_value;
  let mounted;
  let dispose;
  let each_value_26 = ensure_array_like(
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0,
      "cw"
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_26.length; i += 1) {
    each_blocks[i] = create_each_block_26(get_each_context_26(ctx, each_value_26, i));
  }
  let if_block0 = show_if_1 && create_if_block_43(ctx);
  let if_block1 = show_if && create_if_block_42(ctx);
  let if_block2 = (
    /*hasLnaPaPtt*/
    ctx[60] && create_if_block_41(ctx)
  );
  let if_block3 = (
    /*hasLnaPaPttTail*/
    ctx[59] && create_if_block_40(ctx)
  );
  let if_block4 = (
    /*hasCwInVoice*/
    ctx[57] && create_if_block_39(ctx)
  );
  let if_block5 = (
    /*hasSoundcardPtt*/
    ctx[58] && create_if_block_38(ctx)
  );
  let if_block6 = (
    /*pttStatus*/
    ctx[10][`${/*activeSerial*/
    ctx[0]}:r1`] && create_if_block_37(ctx)
  );
  let if_block7 = (
    /*hasR2*/
    ctx[3] && create_if_block_30(ctx)
  );
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "PTT Radio 1";
      t1 = space();
      div7 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "PTT CW:";
      t3 = space();
      div2 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t4 = space();
      if (if_block0) if_block0.c();
      t5 = space();
      if (if_block1) if_block1.c();
      t6 = space();
      if (if_block2) if_block2.c();
      t7 = space();
      if (if_block3) if_block3.c();
      t8 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "PTT Lead (x 10ms):";
      t10 = space();
      div5 = element("div");
      input0 = element("input");
      t11 = space();
      if (if_block4) if_block4.c();
      t12 = space();
      if (if_block5) if_block5.c();
      t13 = space();
      if (if_block6) if_block6.c();
      t14 = space();
      if (if_block7) if_block7.c();
      t15 = space();
      section1 = element("section");
      div8 = element("div");
      div8.textContent = "Footswitch Sequencer";
      t17 = space();
      div24 = element("div");
      div11 = element("div");
      div9 = element("div");
      div9.textContent = "Mute serial CW:";
      t19 = space();
      div10 = element("div");
      input1 = element("input");
      t20 = space();
      div14 = element("div");
      div12 = element("div");
      div12.textContent = "Mute serial FSK:";
      t22 = space();
      div13 = element("div");
      input2 = element("input");
      t23 = space();
      div17 = element("div");
      div15 = element("div");
      div15.textContent = "Restore serial PTT and audio:";
      t25 = space();
      div16 = element("div");
      input3 = element("input");
      t26 = space();
      div20 = element("div");
      div18 = element("div");
      div18.textContent = "Restore serial CW:";
      t28 = space();
      div19 = element("div");
      input4 = element("input");
      t29 = space();
      div23 = element("div");
      div21 = element("div");
      div21.textContent = "Restore serial FSK:";
      t31 = space();
      div22 = element("div");
      input5 = element("input");
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1PttDelay"
      );
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "panel");
      attr(section0, "class", "section");
      attr(div8, "class", "section-title");
      attr(div9, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "muteCompCw"
      );
      attr(div10, "class", "value");
      attr(div11, "class", "row");
      attr(div12, "class", "label");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "muteCompFsk"
      );
      attr(div13, "class", "value");
      attr(div14, "class", "row");
      attr(div15, "class", "label");
      attr(input3, "type", "checkbox");
      input3.checked = input3_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "restorePtt"
      );
      attr(div16, "class", "value");
      attr(div17, "class", "row");
      attr(div18, "class", "label");
      attr(input4, "type", "checkbox");
      input4.checked = input4_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "restoreCompCw"
      );
      attr(div19, "class", "value");
      attr(div20, "class", "row");
      attr(div21, "class", "label");
      attr(input5, "type", "checkbox");
      input5.checked = input5_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "restoreCompFsk"
      );
      attr(div22, "class", "value");
      attr(div23, "class", "row");
      attr(div24, "class", "panel");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      var _a2;
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div7);
      append(div7, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*activePttR1*/
        (_a2 = ctx[37]) == null ? void 0 : _a2.cw
      );
      append(div7, t4);
      if (if_block0) if_block0.m(div7, null);
      append(div7, t5);
      if (if_block1) if_block1.m(div7, null);
      append(div7, t6);
      if (if_block2) if_block2.m(div7, null);
      append(div7, t7);
      if (if_block3) if_block3.m(div7, null);
      append(div7, t8);
      append(div7, div6);
      append(div6, div4);
      append(div6, t10);
      append(div6, div5);
      append(div5, input0);
      append(div7, t11);
      if (if_block4) if_block4.m(div7, null);
      append(div7, t12);
      if (if_block5) if_block5.m(div7, null);
      append(section0, t13);
      if (if_block6) if_block6.m(section0, null);
      insert(target, t14, anchor);
      if (if_block7) if_block7.m(target, anchor);
      insert(target, t15, anchor);
      insert(target, section1, anchor);
      append(section1, div8);
      append(section1, t17);
      append(section1, div24);
      append(div24, div11);
      append(div11, div9);
      append(div11, t19);
      append(div11, div10);
      append(div10, input1);
      append(div24, t20);
      append(div24, div14);
      append(div14, div12);
      append(div14, t22);
      append(div14, div13);
      append(div13, input2);
      append(div24, t23);
      append(div24, div17);
      append(div17, div15);
      append(div17, t25);
      append(div17, div16);
      append(div16, input3);
      append(div24, t26);
      append(div24, div20);
      append(div20, div18);
      append(div20, t28);
      append(div20, div19);
      append(div19, input4);
      append(div24, t29);
      append(div24, div23);
      append(div23, div21);
      append(div23, t31);
      append(div23, div22);
      append(div22, input5);
      if (!mounted) {
        dispose = [
          listen(
            select,
            "change",
            /*change_handler_44*/
            ctx[236]
          ),
          listen(
            input0,
            "input",
            /*input_handler_16*/
            ctx[243]
          ),
          listen(
            input1,
            "change",
            /*change_handler_58*/
            ctx[256]
          ),
          listen(
            input2,
            "change",
            /*change_handler_59*/
            ctx[257]
          ),
          listen(
            input3,
            "change",
            /*change_handler_60*/
            ctx[258]
          ),
          listen(
            input4,
            "change",
            /*change_handler_61*/
            ctx[259]
          ),
          listen(
            input5,
            "change",
            /*change_handler_62*/
            ctx[260]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c2, _d, _e;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*pttOptionLabels*/
      131072 | dirty[3] & /*pttOptionsFor*/
      128) {
        each_value_26 = ensure_array_like(
          /*pttOptionsFor*/
          ctx2[100](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0,
            "cw"
          )
        );
        let i;
        for (i = 0; i < each_value_26.length; i += 1) {
          const child_ctx = get_each_context_26(ctx2, each_value_26, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_26(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_26.length;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[1] & /*activePttR1*/
      64 | dirty[3] & /*pttOptionsFor*/
      128 && select_value_value !== (select_value_value = /*activePttR1*/
      (_b2 = ctx2[37]) == null ? void 0 : _b2.cw)) {
        select_option(
          select,
          /*activePttR1*/
          (_c2 = ctx2[37]) == null ? void 0 : _c2.cw
        );
      }
      if (dirty[0] & /*activeKeyer*/
      4) show_if_1 = /*pttOptionsFor*/
      ctx2[100](
        /*activeKeyer*/
        ((_d = ctx2[2]) == null ? void 0 : _d.type) ?? 0,
        "voice"
      ).length;
      if (show_if_1) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_43(ctx2);
          if_block0.c();
          if_block0.m(div7, t5);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[0] & /*activeKeyer*/
      4) show_if = /*pttOptionsFor*/
      ctx2[100](
        /*activeKeyer*/
        ((_e = ctx2[2]) == null ? void 0 : _e.type) ?? 0,
        "digital"
      ).length;
      if (show_if) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_42(ctx2);
          if_block1.c();
          if_block1.m(div7, t6);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (
        /*hasLnaPaPtt*/
        ctx2[60]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_41(ctx2);
          if_block2.c();
          if_block2.m(div7, t7);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (
        /*hasLnaPaPttTail*/
        ctx2[59]
      ) {
        if (if_block3) {
          if_block3.p(ctx2, dirty);
        } else {
          if_block3 = create_if_block_40(ctx2);
          if_block3.c();
          if_block3.m(div7, t8);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1PttDelay"
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (
        /*hasCwInVoice*/
        ctx2[57]
      ) {
        if (if_block4) {
          if_block4.p(ctx2, dirty);
        } else {
          if_block4 = create_if_block_39(ctx2);
          if_block4.c();
          if_block4.m(div7, t12);
        }
      } else if (if_block4) {
        if_block4.d(1);
        if_block4 = null;
      }
      if (
        /*hasSoundcardPtt*/
        ctx2[58]
      ) {
        if (if_block5) {
          if_block5.p(ctx2, dirty);
        } else {
          if_block5 = create_if_block_38(ctx2);
          if_block5.c();
          if_block5.m(div7, null);
        }
      } else if (if_block5) {
        if_block5.d(1);
        if_block5 = null;
      }
      if (
        /*pttStatus*/
        ctx2[10][`${/*activeSerial*/
        ctx2[0]}:r1`]
      ) {
        if (if_block6) {
          if_block6.p(ctx2, dirty);
        } else {
          if_block6 = create_if_block_37(ctx2);
          if_block6.c();
          if_block6.m(section0, null);
        }
      } else if (if_block6) {
        if_block6.d(1);
        if_block6 = null;
      }
      if (
        /*hasR2*/
        ctx2[3]
      ) {
        if (if_block7) {
          if_block7.p(ctx2, dirty);
        } else {
          if_block7 = create_if_block_30(ctx2);
          if_block7.c();
          if_block7.m(t15.parentNode, t15);
        }
      } else if (if_block7) {
        if_block7.d(1);
        if_block7 = null;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "muteCompCw"
      ))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_checked_value !== (input2_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "muteCompFsk"
      ))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input3_checked_value !== (input3_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "restorePtt"
      ))) {
        input3.checked = input3_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input4_checked_value !== (input4_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "restoreCompCw"
      ))) {
        input4.checked = input4_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input5_checked_value !== (input5_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "restoreCompFsk"
      ))) {
        input5.checked = input5_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t14);
        detach(t15);
        detach(section1);
      }
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      if (if_block2) if_block2.d();
      if (if_block3) if_block3.d();
      if (if_block4) if_block4.d();
      if (if_block5) if_block5.d();
      if (if_block6) if_block6.d();
      if (if_block7) if_block7.d(detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_21(ctx) {
  let t0;
  let t1;
  let if_block2_anchor;
  let if_block0 = (
    /*hasFsk1*/
    ctx[5] && create_if_block_26(ctx)
  );
  let if_block1 = (
    /*hasFsk2*/
    ctx[4] && create_if_block_23(ctx)
  );
  let if_block2 = !/*hasFsk1*/
  ctx[5] && !/*hasFsk2*/
  ctx[4] && create_if_block_22();
  return {
    c() {
      if (if_block0) if_block0.c();
      t0 = space();
      if (if_block1) if_block1.c();
      t1 = space();
      if (if_block2) if_block2.c();
      if_block2_anchor = empty();
    },
    m(target, anchor) {
      if (if_block0) if_block0.m(target, anchor);
      insert(target, t0, anchor);
      if (if_block1) if_block1.m(target, anchor);
      insert(target, t1, anchor);
      if (if_block2) if_block2.m(target, anchor);
      insert(target, if_block2_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (
        /*hasFsk1*/
        ctx2[5]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_26(ctx2);
          if_block0.c();
          if_block0.m(t0.parentNode, t0);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*hasFsk2*/
        ctx2[4]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_23(ctx2);
          if_block1.c();
          if_block1.m(t1.parentNode, t1);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (!/*hasFsk1*/
      ctx2[5] && !/*hasFsk2*/
      ctx2[4]) {
        if (if_block2) ;
        else {
          if_block2 = create_if_block_22();
          if_block2.c();
          if_block2.m(if_block2_anchor.parentNode, if_block2_anchor);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(t0);
        detach(t1);
        detach(if_block2_anchor);
      }
      if (if_block0) if_block0.d(detaching);
      if (if_block1) if_block1.d(detaching);
      if (if_block2) if_block2.d(detaching);
    }
  };
}
function create_if_block_18(ctx) {
  let if_block_anchor;
  function select_block_type_2(ctx2, dirty) {
    if (
      /*hasAux*/
      ctx2[6]
    ) return create_if_block_19;
    return create_else_block_1;
  }
  let current_block_type = select_block_type_2(ctx);
  let if_block = current_block_type(ctx);
  return {
    c() {
      if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type_2(ctx2)) && if_block) {
        if_block.p(ctx2, dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(ctx2);
        if (if_block) {
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if_block.d(detaching);
    }
  };
}
function create_if_block_7(ctx) {
  var _a, _b;
  let show_if = (
    /*hasFlag*/
    ctx[86](
      /*activeSerial*/
      ctx[0],
      "HAS_R1"
    )
  );
  let t0;
  let t1;
  let section;
  let div0;
  let t3;
  let div7;
  let div3;
  let div1;
  let t5;
  let div2;
  let input0;
  let input0_checked_value;
  let t6;
  let div6;
  let div4;
  let t8;
  let div5;
  let select;
  let option;
  let select_value_value;
  let t10;
  let div30;
  let div8;
  let t12;
  let div11;
  let div9;
  let t14;
  let div10;
  let input1;
  let input1_checked_value;
  let t15;
  let div14;
  let div12;
  let t17;
  let div13;
  let input2;
  let input2_value_value;
  let t18;
  let div17;
  let div15;
  let t20;
  let div16;
  let input3;
  let input3_value_value;
  let t21;
  let div20;
  let div18;
  let t23;
  let div19;
  let input4;
  let input4_value_value;
  let t24;
  let div23;
  let div21;
  let t26;
  let div22;
  let input5;
  let input5_value_value;
  let t27;
  let div26;
  let div24;
  let t29;
  let div25;
  let input6;
  let input6_value_value;
  let t30;
  let div29;
  let div27;
  let t32;
  let div28;
  let input7;
  let input7_checked_value;
  let t33;
  let t34;
  let t35;
  let div31;
  let button;
  let t37;
  let mounted;
  let dispose;
  let if_block0 = show_if && create_if_block_15(ctx);
  let if_block1 = (
    /*hasR2*/
    ctx[3] && create_if_block_12(ctx)
  );
  let if_block2 = (
    /*activeRigModeSync*/
    ((_b = (_a = ctx[38]) == null ? void 0 : _a.r1) == null ? void 0 : _b.auto_start) && create_if_block_11(ctx)
  );
  let if_block3 = (
    /*hasR2*/
    ctx[3] && create_if_block_9(ctx)
  );
  let if_block4 = (
    /*rigModeSyncStatus*/
    ctx[9][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_8(ctx)
  );
  return {
    c() {
      var _a2, _b2, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n, _o;
      if (if_block0) if_block0.c();
      t0 = space();
      if (if_block1) if_block1.c();
      t1 = space();
      section = element("section");
      div0 = element("div");
      div0.textContent = "RIG Mode Sync (rigctld)";
      t3 = space();
      div7 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Enabled:";
      t5 = space();
      div2 = element("div");
      input0 = element("input");
      t6 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Backend:";
      t8 = space();
      div5 = element("div");
      select = element("select");
      option = element("option");
      option.textContent = "rigctld";
      t10 = space();
      div30 = element("div");
      div8 = element("div");
      div8.textContent = "Radio 1 Endpoint";
      t12 = space();
      div11 = element("div");
      div9 = element("div");
      div9.textContent = "Enabled:";
      t14 = space();
      div10 = element("div");
      input1 = element("input");
      t15 = space();
      div14 = element("div");
      div12 = element("div");
      div12.textContent = "Host:";
      t17 = space();
      div13 = element("div");
      input2 = element("input");
      t18 = space();
      div17 = element("div");
      div15 = element("div");
      div15.textContent = "Port:";
      t20 = space();
      div16 = element("div");
      input3 = element("input");
      t21 = space();
      div20 = element("div");
      div18 = element("div");
      div18.textContent = "Connect Timeout (ms):";
      t23 = space();
      div19 = element("div");
      input4 = element("input");
      t24 = space();
      div23 = element("div");
      div21 = element("div");
      div21.textContent = "I/O Timeout (ms):";
      t26 = space();
      div22 = element("div");
      input5 = element("input");
      t27 = space();
      div26 = element("div");
      div24 = element("div");
      div24.textContent = "Poll Interval (ms):";
      t29 = space();
      div25 = element("div");
      input6 = element("input");
      t30 = space();
      div29 = element("div");
      div27 = element("div");
      div27.textContent = "Auto-start rigctld:";
      t32 = space();
      div28 = element("div");
      input7 = element("input");
      t33 = space();
      if (if_block2) if_block2.c();
      t34 = space();
      if (if_block3) if_block3.c();
      t35 = space();
      div31 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t37 = space();
      if (if_block4) if_block4.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*activeRigModeSync*/
      ((_a2 = ctx[38]) == null ? void 0 : _a2.enabled);
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      option.__value = "rigctld";
      set_input_value(option, option.__value);
      attr(select, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "panel");
      attr(div8, "class", "section-subtitle");
      attr(div9, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*activeRigModeSync*/
      ((_c = (_b2 = ctx[38]) == null ? void 0 : _b2.r1) == null ? void 0 : _c.enabled);
      attr(div10, "class", "value");
      attr(div11, "class", "row");
      attr(div12, "class", "label");
      attr(input2, "class", "input");
      attr(input2, "type", "text");
      input2.value = input2_value_value = /*activeRigModeSync*/
      ((_e = (_d = ctx[38]) == null ? void 0 : _d.r1) == null ? void 0 : _e.host) ?? "127.0.0.1";
      attr(div13, "class", "value");
      attr(div14, "class", "row");
      attr(div15, "class", "label");
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      attr(input3, "min", "1");
      input3.value = input3_value_value = /*activeRigModeSync*/
      ((_g = (_f = ctx[38]) == null ? void 0 : _f.r1) == null ? void 0 : _g.port) ?? 4532;
      attr(div16, "class", "value");
      attr(div17, "class", "row");
      attr(div18, "class", "label");
      attr(input4, "class", "input");
      attr(input4, "type", "number");
      attr(input4, "min", "100");
      input4.value = input4_value_value = /*activeRigModeSync*/
      ((_i = (_h = ctx[38]) == null ? void 0 : _h.r1) == null ? void 0 : _i.connect_timeout_ms) ?? 1500;
      attr(div19, "class", "value");
      attr(div20, "class", "row");
      attr(div21, "class", "label");
      attr(input5, "class", "input");
      attr(input5, "type", "number");
      attr(input5, "min", "100");
      input5.value = input5_value_value = /*activeRigModeSync*/
      ((_k = (_j = ctx[38]) == null ? void 0 : _j.r1) == null ? void 0 : _k.io_timeout_ms) ?? 1500;
      attr(div22, "class", "value");
      attr(div23, "class", "row");
      attr(div24, "class", "label");
      attr(input6, "class", "input");
      attr(input6, "type", "number");
      attr(input6, "min", "100");
      input6.value = input6_value_value = /*activeRigModeSync*/
      ((_m = (_l = ctx[38]) == null ? void 0 : _l.r1) == null ? void 0 : _m.poll_ms) ?? 500;
      attr(div25, "class", "value");
      attr(div26, "class", "row");
      attr(div27, "class", "label");
      attr(input7, "type", "checkbox");
      input7.checked = input7_checked_value = !!/*activeRigModeSync*/
      ((_o = (_n = ctx[38]) == null ? void 0 : _n.r1) == null ? void 0 : _o.auto_start);
      attr(div28, "class", "value");
      attr(div29, "class", "row");
      attr(div30, "class", "panel");
      set_style(div30, "margin-top", "12px");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div31, "class", "button-row");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a2;
      if (if_block0) if_block0.m(target, anchor);
      insert(target, t0, anchor);
      if (if_block1) if_block1.m(target, anchor);
      insert(target, t1, anchor);
      insert(target, section, anchor);
      append(section, div0);
      append(section, t3);
      append(section, div7);
      append(div7, div3);
      append(div3, div1);
      append(div3, t5);
      append(div3, div2);
      append(div2, input0);
      append(div7, t6);
      append(div7, div6);
      append(div6, div4);
      append(div6, t8);
      append(div6, div5);
      append(div5, select);
      append(select, option);
      select_option(
        select,
        /*activeRigModeSync*/
        ((_a2 = ctx[38]) == null ? void 0 : _a2.backend) || "rigctld"
      );
      append(section, t10);
      append(section, div30);
      append(div30, div8);
      append(div30, t12);
      append(div30, div11);
      append(div11, div9);
      append(div11, t14);
      append(div11, div10);
      append(div10, input1);
      append(div30, t15);
      append(div30, div14);
      append(div14, div12);
      append(div14, t17);
      append(div14, div13);
      append(div13, input2);
      append(div30, t18);
      append(div30, div17);
      append(div17, div15);
      append(div17, t20);
      append(div17, div16);
      append(div16, input3);
      append(div30, t21);
      append(div30, div20);
      append(div20, div18);
      append(div20, t23);
      append(div20, div19);
      append(div19, input4);
      append(div30, t24);
      append(div30, div23);
      append(div23, div21);
      append(div23, t26);
      append(div23, div22);
      append(div22, input5);
      append(div30, t27);
      append(div30, div26);
      append(div26, div24);
      append(div26, t29);
      append(div26, div25);
      append(div25, input6);
      append(div30, t30);
      append(div30, div29);
      append(div29, div27);
      append(div29, t32);
      append(div29, div28);
      append(div28, input7);
      append(div30, t33);
      if (if_block2) if_block2.m(div30, null);
      append(section, t34);
      if (if_block3) if_block3.m(section, null);
      append(section, t35);
      append(section, div31);
      append(div31, button);
      append(section, t37);
      if (if_block4) if_block4.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "change",
            /*change_handler_22*/
            ctx[198]
          ),
          listen(
            select,
            "change",
            /*change_handler_23*/
            ctx[199]
          ),
          listen(
            input1,
            "change",
            /*change_handler_24*/
            ctx[200]
          ),
          listen(
            input2,
            "input",
            /*input_handler_2*/
            ctx[201]
          ),
          listen(
            input3,
            "input",
            /*input_handler_3*/
            ctx[202]
          ),
          listen(
            input4,
            "input",
            /*input_handler_4*/
            ctx[203]
          ),
          listen(
            input5,
            "input",
            /*input_handler_5*/
            ctx[204]
          ),
          listen(
            input6,
            "input",
            /*input_handler_6*/
            ctx[205]
          ),
          listen(
            input7,
            "change",
            /*change_handler_25*/
            ctx[206]
          ),
          listen(
            button,
            "click",
            /*click_handler_2*/
            ctx[216]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n, _o, _p, _q, _r, _s;
      if (dirty[0] & /*activeSerial*/
      1) show_if = /*hasFlag*/
      ctx2[86](
        /*activeSerial*/
        ctx2[0],
        "HAS_R1"
      );
      if (show_if) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_15(ctx2);
          if_block0.c();
          if_block0.m(t0.parentNode, t0);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*hasR2*/
        ctx2[3]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_12(ctx2);
          if_block1.c();
          if_block1.m(t1.parentNode, t1);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input0_checked_value !== (input0_checked_value = !!/*activeRigModeSync*/
      ((_a2 = ctx2[38]) == null ? void 0 : _a2.enabled))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && select_value_value !== (select_value_value = /*activeRigModeSync*/
      ((_b2 = ctx2[38]) == null ? void 0 : _b2.backend) || "rigctld")) {
        select_option(
          select,
          /*activeRigModeSync*/
          ((_c = ctx2[38]) == null ? void 0 : _c.backend) || "rigctld"
        );
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input1_checked_value !== (input1_checked_value = !!/*activeRigModeSync*/
      ((_e = (_d = ctx2[38]) == null ? void 0 : _d.r1) == null ? void 0 : _e.enabled))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input2_value_value !== (input2_value_value = /*activeRigModeSync*/
      ((_g = (_f = ctx2[38]) == null ? void 0 : _f.r1) == null ? void 0 : _g.host) ?? "127.0.0.1") && input2.value !== input2_value_value) {
        input2.value = input2_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input3_value_value !== (input3_value_value = /*activeRigModeSync*/
      ((_i = (_h = ctx2[38]) == null ? void 0 : _h.r1) == null ? void 0 : _i.port) ?? 4532) && input3.value !== input3_value_value) {
        input3.value = input3_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input4_value_value !== (input4_value_value = /*activeRigModeSync*/
      ((_k = (_j = ctx2[38]) == null ? void 0 : _j.r1) == null ? void 0 : _k.connect_timeout_ms) ?? 1500) && input4.value !== input4_value_value) {
        input4.value = input4_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input5_value_value !== (input5_value_value = /*activeRigModeSync*/
      ((_m = (_l = ctx2[38]) == null ? void 0 : _l.r1) == null ? void 0 : _m.io_timeout_ms) ?? 1500) && input5.value !== input5_value_value) {
        input5.value = input5_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input6_value_value !== (input6_value_value = /*activeRigModeSync*/
      ((_o = (_n = ctx2[38]) == null ? void 0 : _n.r1) == null ? void 0 : _o.poll_ms) ?? 500) && input6.value !== input6_value_value) {
        input6.value = input6_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input7_checked_value !== (input7_checked_value = !!/*activeRigModeSync*/
      ((_q = (_p = ctx2[38]) == null ? void 0 : _p.r1) == null ? void 0 : _q.auto_start))) {
        input7.checked = input7_checked_value;
      }
      if (
        /*activeRigModeSync*/
        (_s = (_r = ctx2[38]) == null ? void 0 : _r.r1) == null ? void 0 : _s.auto_start
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_11(ctx2);
          if_block2.c();
          if_block2.m(div30, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (
        /*hasR2*/
        ctx2[3]
      ) {
        if (if_block3) {
          if_block3.p(ctx2, dirty);
        } else {
          if_block3 = create_if_block_9(ctx2);
          if_block3.c();
          if_block3.m(section, t35);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
      if (
        /*rigModeSyncStatus*/
        ctx2[9][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block4) {
          if_block4.p(ctx2, dirty);
        } else {
          if_block4 = create_if_block_8(ctx2);
          if_block4.c();
          if_block4.m(section, null);
        }
      } else if (if_block4) {
        if_block4.d(1);
        if_block4 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(t0);
        detach(t1);
        detach(section);
      }
      if (if_block0) if_block0.d(detaching);
      if (if_block1) if_block1.d(detaching);
      if (if_block2) if_block2.d();
      if (if_block3) if_block3.d();
      if (if_block4) if_block4.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block$1(ctx) {
  let if_block_anchor;
  function select_block_type_1(ctx2, dirty) {
    if (
      /*hasKeyerMode*/
      ctx2[63]
    ) return create_if_block_1$1;
    return create_else_block$1;
  }
  let current_block_type = select_block_type_1(ctx);
  let if_block = current_block_type(ctx);
  return {
    c() {
      if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type_1(ctx2)) && if_block) {
        if_block.p(ctx2, dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(ctx2);
        if (if_block) {
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if_block.d(detaching);
    }
  };
}
function create_each_block_90(ctx) {
  let div2;
  let div0;
  let t0_value = (
    /*entry*/
    ctx[705][0] + ""
  );
  let t0;
  let t1;
  let t2;
  let div1;
  let input;
  let input_value_value;
  let mounted;
  let dispose;
  function input_handler_51(...args) {
    return (
      /*input_handler_51*/
      ctx[435](
        /*entry*/
        ctx[705],
        ...args
      )
    );
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      t0 = text(t0_value);
      t1 = text(":");
      t2 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "class", "input");
      set_style(input, "width", "100%");
      attr(input, "type", "number");
      input.value = input_value_value = /*entry*/
      ctx[705][1];
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div0, t0);
      append(div0, t1);
      append(div2, t2);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(input, "input", input_handler_51);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*allParamForm, activeSerial*/
      8193 && t0_value !== (t0_value = /*entry*/
      ctx[705][0] + "")) set_data(t0, t0_value);
      if (dirty[0] & /*allParamForm, activeSerial*/
      8193 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_value_value !== (input_value_value = /*entry*/
      ctx[705][1]) && input.value !== input_value_value) {
        input.value = input_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_120(ctx) {
  let div;
  let t_value = (
    /*allParamStatus*/
    ctx[14][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*allParamStatus*/
      ctx[14][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*allParamStatus, activeSerial*/
      16385 && t_value !== (t_value = /*allParamStatus*/
      ctx2[14][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*allParamStatus, activeSerial*/
      16385 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*allParamStatus*/
      ctx2[14][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_118(ctx) {
  let th;
  return {
    c() {
      th = element("th");
      th.textContent = "BPF";
    },
    m(target, anchor) {
      insert(target, th, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(th);
      }
    }
  };
}
function create_if_block_117(ctx) {
  let th;
  return {
    c() {
      th = element("th");
      th.textContent = "SEQ";
    },
    m(target, anchor) {
      insert(target, th, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(th);
      }
    }
  };
}
function create_else_block_21(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_131() {
    return (
      /*change_handler_131*/
      ctx[405](
        /*bnd*/
        ctx[672]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smBndSelected*/
      (ctx[34][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*bnd*/
        ctx[672].id
      );
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_131);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndSelected, activeBnds, activeAntsAndGroups*/
      139272 && input_checked_value !== (input_checked_value = /*smBndSelected*/
      (ctx[34][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*bnd*/
        ctx[672].id
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_116(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_20(ctx) {
  let td0;
  let t0_value = (
    /*bnd*/
    (ctx[672].display || "") + ""
  );
  let t0;
  let t1;
  let td1;
  let t2_value = (
    /*bnd*/
    (ctx[672].low_freq || 0) + ""
  );
  let t2;
  let t3;
  let td2;
  let t4_value = (
    /*bnd*/
    (ctx[672].high_freq || 0) + ""
  );
  let t4;
  let t5;
  let td3;
  let t6_value = (
    /*bnd*/
    (ctx[672].bcd_code || 0) + ""
  );
  let t6;
  let t7;
  let td4;
  let t8_value = (
    /*bnd*/
    ctx[672].pa_power ? "Yes" : "No"
  );
  let t8;
  let t9;
  let td5;
  let t10_value = (
    /*bnd*/
    ctx[672].keyout ? "Yes" : "No"
  );
  let t10;
  let t11;
  let td6;
  let t12;
  let t13;
  let if_block1_anchor;
  let if_block0 = (
    /*bpfOuts*/
    ctx[668].length > 0 && create_if_block_115(ctx)
  );
  let if_block1 = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_114(ctx)
  );
  return {
    c() {
      td0 = element("td");
      t0 = text(t0_value);
      t1 = space();
      td1 = element("td");
      t2 = text(t2_value);
      t3 = space();
      td2 = element("td");
      t4 = text(t4_value);
      t5 = space();
      td3 = element("td");
      t6 = text(t6_value);
      t7 = space();
      td4 = element("td");
      t8 = text(t8_value);
      t9 = space();
      td5 = element("td");
      t10 = text(t10_value);
      t11 = space();
      td6 = element("td");
      t12 = space();
      if (if_block0) if_block0.c();
      t13 = space();
      if (if_block1) if_block1.c();
      if_block1_anchor = empty();
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, t0);
      insert(target, t1, anchor);
      insert(target, td1, anchor);
      append(td1, t2);
      insert(target, t3, anchor);
      insert(target, td2, anchor);
      append(td2, t4);
      insert(target, t5, anchor);
      insert(target, td3, anchor);
      append(td3, t6);
      insert(target, t7, anchor);
      insert(target, td4, anchor);
      append(td4, t8);
      insert(target, t9, anchor);
      insert(target, td5, anchor);
      append(td5, t10);
      insert(target, t11, anchor);
      insert(target, td6, anchor);
      insert(target, t12, anchor);
      if (if_block0) if_block0.m(target, anchor);
      insert(target, t13, anchor);
      if (if_block1) if_block1.m(target, anchor);
      insert(target, if_block1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeBnds*/
      131072 && t0_value !== (t0_value = /*bnd*/
      (ctx2[672].display || "") + "")) set_data(t0, t0_value);
      if (dirty[1] & /*activeBnds*/
      131072 && t2_value !== (t2_value = /*bnd*/
      (ctx2[672].low_freq || 0) + "")) set_data(t2, t2_value);
      if (dirty[1] & /*activeBnds*/
      131072 && t4_value !== (t4_value = /*bnd*/
      (ctx2[672].high_freq || 0) + "")) set_data(t4, t4_value);
      if (dirty[1] & /*activeBnds*/
      131072 && t6_value !== (t6_value = /*bnd*/
      (ctx2[672].bcd_code || 0) + "")) set_data(t6, t6_value);
      if (dirty[1] & /*activeBnds*/
      131072 && t8_value !== (t8_value = /*bnd*/
      ctx2[672].pa_power ? "Yes" : "No")) set_data(t8, t8_value);
      if (dirty[1] & /*activeBnds*/
      131072 && t10_value !== (t10_value = /*bnd*/
      ctx2[672].keyout ? "Yes" : "No")) set_data(t10, t10_value);
      if (
        /*bpfOuts*/
        ctx2[668].length > 0
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_115(ctx2);
          if_block0.c();
          if_block0.m(t13.parentNode, t13);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*seqOuts*/
        ctx2[669].length > 0
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_114(ctx2);
          if_block1.c();
          if_block1.m(if_block1_anchor.parentNode, if_block1_anchor);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t1);
        detach(td1);
        detach(t3);
        detach(td2);
        detach(t5);
        detach(td3);
        detach(t7);
        detach(td4);
        detach(t9);
        detach(td5);
        detach(t11);
        detach(td6);
        detach(t12);
        detach(t13);
        detach(if_block1_anchor);
      }
      if (if_block0) if_block0.d(detaching);
      if (if_block1) if_block1.d(detaching);
    }
  };
}
function create_if_block_111(ctx) {
  let td0;
  let input0;
  let input0_value_value;
  let t0;
  let td1;
  let input1;
  let input1_value_value;
  let t1;
  let td2;
  let input2;
  let input2_value_value;
  let t2;
  let td3;
  let input3;
  let input3_value_value;
  let t3;
  let td4;
  let input4;
  let input4_checked_value;
  let t4;
  let td5;
  let input5;
  let input5_checked_value;
  let t5;
  let td6;
  let t6;
  let t7;
  let if_block1_anchor;
  let mounted;
  let dispose;
  function input_handler_47(...args) {
    return (
      /*input_handler_47*/
      ctx[406](
        /*bnd*/
        ctx[672],
        ...args
      )
    );
  }
  function input_handler_48(...args) {
    return (
      /*input_handler_48*/
      ctx[407](
        /*bnd*/
        ctx[672],
        ...args
      )
    );
  }
  function input_handler_49(...args) {
    return (
      /*input_handler_49*/
      ctx[408](
        /*bnd*/
        ctx[672],
        ...args
      )
    );
  }
  function input_handler_50(...args) {
    return (
      /*input_handler_50*/
      ctx[409](
        /*bnd*/
        ctx[672],
        ...args
      )
    );
  }
  function change_handler_132(...args) {
    return (
      /*change_handler_132*/
      ctx[410](
        /*bnd*/
        ctx[672],
        ...args
      )
    );
  }
  function change_handler_133(...args) {
    return (
      /*change_handler_133*/
      ctx[411](
        /*bnd*/
        ctx[672],
        ...args
      )
    );
  }
  let if_block0 = (
    /*bpfOuts*/
    ctx[668].length > 0 && create_if_block_113(ctx)
  );
  let if_block1 = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_112(ctx)
  );
  return {
    c() {
      td0 = element("td");
      input0 = element("input");
      t0 = space();
      td1 = element("td");
      input1 = element("input");
      t1 = space();
      td2 = element("td");
      input2 = element("input");
      t2 = space();
      td3 = element("td");
      input3 = element("input");
      t3 = space();
      td4 = element("td");
      input4 = element("input");
      t4 = space();
      td5 = element("td");
      input5 = element("input");
      t5 = space();
      td6 = element("td");
      t6 = space();
      if (if_block0) if_block0.c();
      t7 = space();
      if (if_block1) if_block1.c();
      if_block1_anchor = empty();
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "10");
      input0.value = input0_value_value = /*ef*/
      ctx[630].display;
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      set_style(input1, "width", "90px");
      input1.value = input1_value_value = /*ef*/
      ctx[630].low_freq;
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      set_style(input2, "width", "90px");
      input2.value = input2_value_value = /*ef*/
      ctx[630].high_freq;
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      set_style(input3, "width", "40px");
      input3.value = input3_value_value = /*ef*/
      ctx[630].bcd_code;
      attr(input4, "type", "checkbox");
      input4.checked = input4_checked_value = /*ef*/
      ctx[630].pa_power === 1;
      attr(input5, "type", "checkbox");
      input5.checked = input5_checked_value = /*ef*/
      ctx[630].keyout === 1;
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, input0);
      insert(target, t0, anchor);
      insert(target, td1, anchor);
      append(td1, input1);
      insert(target, t1, anchor);
      insert(target, td2, anchor);
      append(td2, input2);
      insert(target, t2, anchor);
      insert(target, td3, anchor);
      append(td3, input3);
      insert(target, t3, anchor);
      insert(target, td4, anchor);
      append(td4, input4);
      insert(target, t4, anchor);
      insert(target, td5, anchor);
      append(td5, input5);
      insert(target, t5, anchor);
      insert(target, td6, anchor);
      insert(target, t6, anchor);
      if (if_block0) if_block0.m(target, anchor);
      insert(target, t7, anchor);
      if (if_block1) if_block1.m(target, anchor);
      insert(target, if_block1_anchor, anchor);
      if (!mounted) {
        dispose = [
          listen(input0, "input", input_handler_47),
          listen(input1, "input", input_handler_48),
          listen(input2, "input", input_handler_49),
          listen(input3, "input", input_handler_50),
          listen(input4, "change", change_handler_132),
          listen(input5, "change", change_handler_133)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input0_value_value !== (input0_value_value = /*ef*/
      ctx[630].display) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input1_value_value !== (input1_value_value = /*ef*/
      ctx[630].low_freq) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input2_value_value !== (input2_value_value = /*ef*/
      ctx[630].high_freq) && input2.value !== input2_value_value) {
        input2.value = input2_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input3_value_value !== (input3_value_value = /*ef*/
      ctx[630].bcd_code) && input3.value !== input3_value_value) {
        input3.value = input3_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input4_checked_value !== (input4_checked_value = /*ef*/
      ctx[630].pa_power === 1)) {
        input4.checked = input4_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input5_checked_value !== (input5_checked_value = /*ef*/
      ctx[630].keyout === 1)) {
        input5.checked = input5_checked_value;
      }
      if (
        /*bpfOuts*/
        ctx[668].length > 0
      ) {
        if (if_block0) {
          if_block0.p(ctx, dirty);
        } else {
          if_block0 = create_if_block_113(ctx);
          if_block0.c();
          if_block0.m(t7.parentNode, t7);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*seqOuts*/
        ctx[669].length > 0
      ) {
        if (if_block1) {
          if_block1.p(ctx, dirty);
        } else {
          if_block1 = create_if_block_112(ctx);
          if_block1.c();
          if_block1.m(if_block1_anchor.parentNode, if_block1_anchor);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t0);
        detach(td1);
        detach(t1);
        detach(td2);
        detach(t2);
        detach(td3);
        detach(t3);
        detach(td4);
        detach(t4);
        detach(td5);
        detach(t5);
        detach(td6);
        detach(t6);
        detach(t7);
        detach(if_block1_anchor);
      }
      if (if_block0) if_block0.d(detaching);
      if (if_block1) if_block1.d(detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_115(ctx) {
  let td;
  let table;
  let tr0;
  let t;
  let tr1;
  let each_value_89 = ensure_array_like(
    /*bpfOuts*/
    ctx[668]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_89.length; i += 1) {
    each_blocks_1[i] = create_each_block_89(get_each_context_89(ctx, each_value_89, i));
  }
  let each_value_88 = ensure_array_like(
    /*bpfOuts*/
    ctx[668]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_88.length; i += 1) {
    each_blocks[i] = create_each_block_88(get_each_context_88(ctx, each_value_88, i));
  }
  return {
    c() {
      td = element("td");
      table = element("table");
      tr0 = element("tr");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t = space();
      tr1 = element("tr");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(table, "class", "ant-table");
      set_style(table, "margin", "0");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, table);
      append(table, tr0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(tr0, null);
        }
      }
      append(table, t);
      append(table, tr1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tr1, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeBpfOutputs*/
      65536) {
        each_value_89 = ensure_array_like(
          /*bpfOuts*/
          ctx2[668]
        );
        let i;
        for (i = 0; i < each_value_89.length; i += 1) {
          const child_ctx = get_each_context_89(ctx2, each_value_89, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_89(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(tr0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_89.length;
      }
      if (dirty[1] & /*activeBnds, activeBpfOutputs*/
      196608) {
        each_value_88 = ensure_array_like(
          /*bpfOuts*/
          ctx2[668]
        );
        let i;
        for (i = 0; i < each_value_88.length; i += 1) {
          const child_ctx = get_each_context_88(ctx2, each_value_88, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_88(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(tr1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_88.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_each_block_89(ctx) {
  let td;
  let t_value = (
    /*bo*/
    ctx[692] + ""
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
      set_style(td, "font-size", "0.85em");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeBpfOutputs*/
      65536 && t_value !== (t_value = /*bo*/
      ctx2[692] + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_88(ctx) {
  var _a;
  let td;
  let t_value = (
    /*bnd*/
    ((_a = ctx[672].bpf_seq) == null ? void 0 : _a[
      /*bo*/
      ctx[692]
    ]) || 0 ? "Yes" : "No"
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty[1] & /*activeBnds, activeBpfOutputs*/
      196608 && t_value !== (t_value = /*bnd*/
      ((_a2 = ctx2[672].bpf_seq) == null ? void 0 : _a2[
        /*bo*/
        ctx2[692]
      ]) || 0 ? "Yes" : "No")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_114(ctx) {
  let td;
  let table;
  let tr0;
  let t;
  let tr1;
  let each_value_87 = ensure_array_like(
    /*seqOuts*/
    ctx[669]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_87.length; i += 1) {
    each_blocks_1[i] = create_each_block_87(get_each_context_87(ctx, each_value_87, i));
  }
  let each_value_86 = ensure_array_like(
    /*seqOuts*/
    ctx[669]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_86.length; i += 1) {
    each_blocks[i] = create_each_block_86(get_each_context_86(ctx, each_value_86, i));
  }
  return {
    c() {
      td = element("td");
      table = element("table");
      tr0 = element("tr");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t = space();
      tr1 = element("tr");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(table, "class", "ant-table");
      set_style(table, "margin", "0");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, table);
      append(table, tr0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(tr0, null);
        }
      }
      append(table, t);
      append(table, tr1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tr1, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeSeqOutputs*/
      32768) {
        each_value_87 = ensure_array_like(
          /*seqOuts*/
          ctx2[669]
        );
        let i;
        for (i = 0; i < each_value_87.length; i += 1) {
          const child_ctx = get_each_context_87(ctx2, each_value_87, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_87(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(tr0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_87.length;
      }
      if (dirty[1] & /*activeBnds, activeSeqOutputs*/
      163840) {
        each_value_86 = ensure_array_like(
          /*seqOuts*/
          ctx2[669]
        );
        let i;
        for (i = 0; i < each_value_86.length; i += 1) {
          const child_ctx = get_each_context_86(ctx2, each_value_86, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_86(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(tr1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_86.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_each_block_87(ctx) {
  let td;
  let t_value = (
    /*so*/
    ctx[687] + ""
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
      set_style(td, "font-size", "0.85em");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeSeqOutputs*/
      32768 && t_value !== (t_value = /*so*/
      ctx2[687] + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_86(ctx) {
  var _a;
  let td;
  let t_value = (
    /*bnd*/
    ((_a = ctx[672].bpf_seq) == null ? void 0 : _a[
      /*so*/
      ctx[687]
    ]) || 0 ? "Yes" : "No"
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty[1] & /*activeBnds, activeSeqOutputs*/
      163840 && t_value !== (t_value = /*bnd*/
      ((_a2 = ctx2[672].bpf_seq) == null ? void 0 : _a2[
        /*so*/
        ctx2[687]
      ]) || 0 ? "Yes" : "No")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_113(ctx) {
  let td;
  let table;
  let tr0;
  let t;
  let tr1;
  let each_value_85 = ensure_array_like(
    /*bpfOuts*/
    ctx[668]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_85.length; i += 1) {
    each_blocks_1[i] = create_each_block_85(get_each_context_85(ctx, each_value_85, i));
  }
  let each_value_84 = ensure_array_like(
    /*bpfOuts*/
    ctx[668]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_84.length; i += 1) {
    each_blocks[i] = create_each_block_84(get_each_context_84(ctx, each_value_84, i));
  }
  return {
    c() {
      td = element("td");
      table = element("table");
      tr0 = element("tr");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t = space();
      tr1 = element("tr");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(table, "class", "ant-table");
      set_style(table, "margin", "0");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, table);
      append(table, tr0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(tr0, null);
        }
      }
      append(table, t);
      append(table, tr1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tr1, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeBpfOutputs*/
      65536) {
        each_value_85 = ensure_array_like(
          /*bpfOuts*/
          ctx2[668]
        );
        let i;
        for (i = 0; i < each_value_85.length; i += 1) {
          const child_ctx = get_each_context_85(ctx2, each_value_85, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_85(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(tr0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_85.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeBpfOutputs*/
      196612) {
        each_value_84 = ensure_array_like(
          /*bpfOuts*/
          ctx2[668]
        );
        let i;
        for (i = 0; i < each_value_84.length; i += 1) {
          const child_ctx = get_each_context_84(ctx2, each_value_84, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_84(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(tr1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_84.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_each_block_85(ctx) {
  let td;
  let t_value = (
    /*bo*/
    ctx[692] + ""
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
      set_style(td, "font-size", "0.85em");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeBpfOutputs*/
      65536 && t_value !== (t_value = /*bo*/
      ctx2[692] + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_84(ctx) {
  let td;
  let input;
  let input_checked_value;
  let t;
  let mounted;
  let dispose;
  function change_handler_134(...args) {
    return (
      /*change_handler_134*/
      ctx[412](
        /*bnd*/
        ctx[672],
        /*bo*/
        ctx[692],
        ...args
      )
    );
  }
  return {
    c() {
      var _a;
      td = element("td");
      input = element("input");
      t = space();
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*ef*/
      (((_a = ctx[630].bpf_seq) == null ? void 0 : _a[
        /*bo*/
        ctx[692]
      ]) || 0) === 1;
      set_style(td, "padding", "1px 4px");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      insert(target, t, anchor);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_134);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a;
      ctx = new_ctx;
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeBpfOutputs, activeAntsAndGroups*/
      204804 && input_checked_value !== (input_checked_value = /*ef*/
      (((_a = ctx[630].bpf_seq) == null ? void 0 : _a[
        /*bo*/
        ctx[692]
      ]) || 0) === 1)) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
        detach(t);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_112(ctx) {
  let td;
  let table;
  let tr0;
  let t;
  let tr1;
  let each_value_83 = ensure_array_like(
    /*seqOuts*/
    ctx[669]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_83.length; i += 1) {
    each_blocks_1[i] = create_each_block_83(get_each_context_83(ctx, each_value_83, i));
  }
  let each_value_82 = ensure_array_like(
    /*seqOuts*/
    ctx[669]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_82.length; i += 1) {
    each_blocks[i] = create_each_block_82(get_each_context_82(ctx, each_value_82, i));
  }
  return {
    c() {
      td = element("td");
      table = element("table");
      tr0 = element("tr");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t = space();
      tr1 = element("tr");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(table, "class", "ant-table");
      set_style(table, "margin", "0");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, table);
      append(table, tr0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(tr0, null);
        }
      }
      append(table, t);
      append(table, tr1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tr1, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeSeqOutputs*/
      32768) {
        each_value_83 = ensure_array_like(
          /*seqOuts*/
          ctx2[669]
        );
        let i;
        for (i = 0; i < each_value_83.length; i += 1) {
          const child_ctx = get_each_context_83(ctx2, each_value_83, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_83(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(tr0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_83.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeSeqOutputs*/
      163844) {
        each_value_82 = ensure_array_like(
          /*seqOuts*/
          ctx2[669]
        );
        let i;
        for (i = 0; i < each_value_82.length; i += 1) {
          const child_ctx = get_each_context_82(ctx2, each_value_82, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_82(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(tr1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_82.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_each_block_83(ctx) {
  let td;
  let t_value = (
    /*so*/
    ctx[687] + ""
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
      set_style(td, "font-size", "0.85em");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeSeqOutputs*/
      32768 && t_value !== (t_value = /*so*/
      ctx2[687] + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_82(ctx) {
  let td;
  let input;
  let input_checked_value;
  let t;
  let mounted;
  let dispose;
  function change_handler_135(...args) {
    return (
      /*change_handler_135*/
      ctx[413](
        /*bnd*/
        ctx[672],
        /*so*/
        ctx[687],
        ...args
      )
    );
  }
  return {
    c() {
      var _a;
      td = element("td");
      input = element("input");
      t = space();
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*ef*/
      (((_a = ctx[630].bpf_seq) == null ? void 0 : _a[
        /*so*/
        ctx[687]
      ]) || 0) === 1;
      set_style(td, "padding", "1px 4px");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      insert(target, t, anchor);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_135);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a;
      ctx = new_ctx;
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeSeqOutputs, activeAntsAndGroups*/
      172036 && input_checked_value !== (input_checked_value = /*ef*/
      (((_a = ctx[630].bpf_seq) == null ? void 0 : _a[
        /*so*/
        ctx[687]
      ]) || 0) === 1)) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
        detach(t);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_else_block_19(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_136() {
    return (
      /*change_handler_136*/
      ctx[414](
        /*bnd*/
        ctx[672],
        /*ref*/
        ctx[647]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smBndSelected*/
      (ctx[34][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(`${/*bnd*/
      ctx[672].id}:${/*ref*/
      ctx[647].id}`);
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_136);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndSelected, activeBnds, activeAntsAndGroups*/
      139272 && input_checked_value !== (input_checked_value = /*smBndSelected*/
      (ctx[34][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(`${/*bnd*/
      ctx[672].id}:${/*ref*/
      ctx[647].id}`))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_110(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_18(ctx) {
  let td0;
  let t0_value = (
    /*smObjTypedDisplay*/
    ctx[148](
      /*activeSerial*/
      ctx[0],
      /*ref*/
      ctx[647].dest_id
    ) + ""
  );
  let t0;
  let t1;
  let td1;
  let t2;
  let t3_value = (
    /*ref*/
    ctx[647].rxonly ? "Yes" : "No"
  );
  let t3;
  return {
    c() {
      td0 = element("td");
      t0 = text(t0_value);
      t1 = space();
      td1 = element("td");
      t2 = text("RX only ");
      t3 = text(t3_value);
      attr(td0, "colspan", "3");
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, t0);
      insert(target, t1, anchor);
      insert(target, td1, anchor);
      append(td1, t2);
      append(td1, t3);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeBnds*/
      131072 && t0_value !== (t0_value = /*smObjTypedDisplay*/
      ctx2[148](
        /*activeSerial*/
        ctx2[0],
        /*ref*/
        ctx2[647].dest_id
      ) + "")) set_data(t0, t0_value);
      if (dirty[1] & /*activeBnds*/
      131072 && t3_value !== (t3_value = /*ref*/
      ctx2[647].rxonly ? "Yes" : "No")) set_data(t3, t3_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t1);
        detach(td1);
      }
    }
  };
}
function create_if_block_109(ctx) {
  let td0;
  let select;
  let select_value_value;
  let t0;
  let td1;
  let t1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  let each_value_81 = ensure_array_like(
    /*antGrpObjs*/
    ctx[671]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_81.length; i += 1) {
    each_blocks[i] = create_each_block_81(get_each_context_81(ctx, each_value_81, i));
  }
  function change_handler_137(...args) {
    return (
      /*change_handler_137*/
      ctx[415](
        /*bnd*/
        ctx[672],
        /*ref*/
        ctx[647],
        ...args
      )
    );
  }
  function change_handler_138(...args) {
    return (
      /*change_handler_138*/
      ctx[416](
        /*bnd*/
        ctx[672],
        /*ref*/
        ctx[647],
        ...args
      )
    );
  }
  return {
    c() {
      td0 = element("td");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t0 = space();
      td1 = element("td");
      t1 = text("RX only ");
      input = element("input");
      attr(select, "class", "select");
      attr(td0, "colspan", "3");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*rf*/
      (ctx[650].rxonly || 0) === 1;
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*rf*/
        ctx[650].dest_id
      );
      insert(target, t0, anchor);
      insert(target, td1, anchor);
      append(td1, t1);
      append(td1, input);
      if (!mounted) {
        dispose = [
          listen(select, "change", change_handler_137),
          listen(input, "change", change_handler_138)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[1] & /*activeAntsAndGroups*/
      8192) {
        each_value_81 = ensure_array_like(
          /*antGrpObjs*/
          ctx[671]
        );
        let i;
        for (i = 0; i < each_value_81.length; i += 1) {
          const child_ctx = get_each_context_81(ctx, each_value_81, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_81(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_81.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && select_value_value !== (select_value_value = /*rf*/
      ctx[650].dest_id)) {
        select_option(
          select,
          /*rf*/
          ctx[650].dest_id
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndEditForms, activeBnds, activeAntsAndGroups*/
      139268 && input_checked_value !== (input_checked_value = /*rf*/
      (ctx[650].rxonly || 0) === 1)) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t0);
        detach(td1);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_81(ctx) {
  let option;
  let t0_value = (
    /*obj*/
    ctx[644].type === 0 ? "ANT" : (
      /*obj*/
      ctx[644].type === 1 && /*obj*/
      ctx[644].virtual_rotator ? "VIR" : "GRP"
    )
  );
  let t0;
  let t1;
  let t2_value = (
    /*obj*/
    (ctx[644].display || /*obj*/
    ctx[644].label || `#${/*obj*/
    ctx[644].id}`) + ""
  );
  let t2;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t0 = text(t0_value);
      t1 = text(": ");
      t2 = text(t2_value);
      option.__value = option_value_value = /*obj*/
      ctx[644].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t0);
      append(option, t1);
      append(option, t2);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntsAndGroups*/
      8192 && t0_value !== (t0_value = /*obj*/
      ctx2[644].type === 0 ? "ANT" : (
        /*obj*/
        ctx2[644].type === 1 && /*obj*/
        ctx2[644].virtual_rotator ? "VIR" : "GRP"
      ))) set_data(t0, t0_value);
      if (dirty[1] & /*activeAntsAndGroups*/
      8192 && t2_value !== (t2_value = /*obj*/
      (ctx2[644].display || /*obj*/
      ctx2[644].label || `#${/*obj*/
      ctx2[644].id}`) + "")) set_data(t2, t2_value);
      if (dirty[1] & /*activeAntsAndGroups*/
      8192 && option_value_value !== (option_value_value = /*obj*/
      ctx2[644].id)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_108(ctx) {
  let table;
  let tr0;
  let t;
  let tr1;
  let each_value_80 = ensure_array_like(
    /*antOuts*/
    ctx[670]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_80.length; i += 1) {
    each_blocks_1[i] = create_each_block_80(get_each_context_80(ctx, each_value_80, i));
  }
  let each_value_79 = ensure_array_like(
    /*antOuts*/
    ctx[670]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_79.length; i += 1) {
    each_blocks[i] = create_each_block_79(get_each_context_79(ctx, each_value_79, i));
  }
  return {
    c() {
      table = element("table");
      tr0 = element("tr");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t = space();
      tr1 = element("tr");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(table, "class", "ant-table");
      set_style(table, "margin", "0");
    },
    m(target, anchor) {
      insert(target, table, anchor);
      append(table, tr0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(tr0, null);
        }
      }
      append(table, t);
      append(table, tr1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tr1, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntOutputs*/
      16384) {
        each_value_80 = ensure_array_like(
          /*antOuts*/
          ctx2[670]
        );
        let i;
        for (i = 0; i < each_value_80.length; i += 1) {
          const child_ctx = get_each_context_80(ctx2, each_value_80, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_80(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(tr0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_80.length;
      }
      if (dirty[1] & /*activeAllObjs, activeBnds, activeAntOutputs*/
      409600) {
        each_value_79 = ensure_array_like(
          /*antOuts*/
          ctx2[670]
        );
        let i;
        for (i = 0; i < each_value_79.length; i += 1) {
          const child_ctx = get_each_context_79(ctx2, each_value_79, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_79(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(tr1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_79.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(table);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_each_block_80(ctx) {
  let td;
  let t_value = (
    /*ao*/
    ctx[680] + ""
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
      set_style(td, "font-size", "0.85em");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntOutputs*/
      16384 && t_value !== (t_value = /*ao*/
      ctx2[680] + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_79(ctx) {
  var _a;
  let td;
  let t_value = (
    /*destObj*/
    ((_a = ctx[677].output) == null ? void 0 : _a[
      /*ao*/
      ctx[680]
    ]) || 0 ? "Yes" : "No"
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
      set_style(td, "padding", "1px 4px");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty[1] & /*activeAllObjs, activeBnds, activeAntOutputs*/
      409600 && t_value !== (t_value = /*destObj*/
      ((_a2 = ctx2[677].output) == null ? void 0 : _a2[
        /*ao*/
        ctx2[680]
      ]) || 0 ? "Yes" : "No")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_107(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_106(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_78(key_1, ctx) {
  let tr;
  let t0;
  let td0;
  let t1;
  let t2;
  let td1;
  let t3;
  let td2;
  let t4;
  let t5;
  let t6;
  function select_block_type_19(ctx2, dirty) {
    if (
      /*bndMode*/
      ctx2[666] === "edit"
    ) return create_if_block_110;
    return create_else_block_19;
  }
  let current_block_type = select_block_type_19(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_20(ctx2, dirty) {
    if (
      /*bndMode*/
      ctx2[666] === "edit"
    ) return create_if_block_109;
    return create_else_block_18;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_109) return get_if_ctx_9(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_20(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  let if_block2 = (
    /*destObj*/
    ctx[677] && /*destObj*/
    ctx[677].type === 0 && create_if_block_108(ctx)
  );
  let if_block3 = (
    /*bpfOuts*/
    ctx[668].length > 0 && create_if_block_107()
  );
  let if_block4 = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_106()
  );
  return {
    key: key_1,
    first: null,
    c() {
      tr = element("tr");
      if_block0.c();
      t0 = space();
      td0 = element("td");
      t1 = space();
      if_block1.c();
      t2 = space();
      td1 = element("td");
      t3 = space();
      td2 = element("td");
      if (if_block2) if_block2.c();
      t4 = space();
      if (if_block3) if_block3.c();
      t5 = space();
      if (if_block4) if_block4.c();
      t6 = space();
      this.first = tr;
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      if_block0.m(tr, null);
      append(tr, t0);
      append(tr, td0);
      append(tr, t1);
      if_block1.m(tr, null);
      append(tr, t2);
      append(tr, td1);
      append(tr, t3);
      append(tr, td2);
      if (if_block2) if_block2.m(td2, null);
      append(tr, t4);
      if (if_block3) if_block3.m(tr, null);
      append(tr, t5);
      if (if_block4) if_block4.m(tr, null);
      append(tr, t6);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_19(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr, t0);
        }
      }
      if (current_block_type_1 === (current_block_type_1 = select_block_type_20(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr, t2);
        }
      }
      if (
        /*destObj*/
        ctx[677] && /*destObj*/
        ctx[677].type === 0
      ) {
        if (if_block2) {
          if_block2.p(ctx, dirty);
        } else {
          if_block2 = create_if_block_108(ctx);
          if_block2.c();
          if_block2.m(td2, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (
        /*bpfOuts*/
        ctx[668].length > 0
      ) {
        if (if_block3) ;
        else {
          if_block3 = create_if_block_107();
          if_block3.c();
          if_block3.m(tr, t5);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
      if (
        /*seqOuts*/
        ctx[669].length > 0
      ) {
        if (if_block4) ;
        else {
          if_block4 = create_if_block_106();
          if_block4.c();
          if_block4.m(tr, t6);
        }
      } else if (if_block4) {
        if_block4.d(1);
        if_block4 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      if_block0.d();
      if_block1.d();
      if (if_block2) if_block2.d();
      if (if_block3) if_block3.d();
      if (if_block4) if_block4.d();
    }
  };
}
function create_else_block_17(ctx) {
  let td0;
  let t0;
  let t1;
  let t2;
  let td1;
  let if_block0 = (
    /*bpfOuts*/
    ctx[668].length > 0 && create_if_block_105()
  );
  let if_block1 = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_104()
  );
  let if_block2 = !/*bndMode*/
  ctx[666] && !/*bndAddRefState*/
  ctx[667] && create_if_block_103(ctx);
  return {
    c() {
      td0 = element("td");
      t0 = space();
      if (if_block0) if_block0.c();
      t1 = space();
      if (if_block1) if_block1.c();
      t2 = space();
      td1 = element("td");
      if (if_block2) if_block2.c();
      attr(td0, "colspan", "5");
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      insert(target, t0, anchor);
      if (if_block0) if_block0.m(target, anchor);
      insert(target, t1, anchor);
      if (if_block1) if_block1.m(target, anchor);
      insert(target, t2, anchor);
      insert(target, td1, anchor);
      if (if_block2) if_block2.m(td1, null);
    },
    p(ctx2, dirty) {
      if (
        /*bpfOuts*/
        ctx2[668].length > 0
      ) {
        if (if_block0) ;
        else {
          if_block0 = create_if_block_105();
          if_block0.c();
          if_block0.m(t1.parentNode, t1);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*seqOuts*/
        ctx2[669].length > 0
      ) {
        if (if_block1) ;
        else {
          if_block1 = create_if_block_104();
          if_block1.c();
          if_block1.m(t2.parentNode, t2);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (!/*bndMode*/
      ctx2[666] && !/*bndAddRefState*/
      ctx2[667]) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_103(ctx2);
          if_block2.c();
          if_block2.m(td1, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t0);
        detach(t1);
        detach(t2);
        detach(td1);
      }
      if (if_block0) if_block0.d(detaching);
      if (if_block1) if_block1.d(detaching);
      if (if_block2) if_block2.d();
    }
  };
}
function create_if_block_101(ctx) {
  let td0;
  let select;
  let t0;
  let td1;
  let t1;
  let td2;
  let t2;
  let td3;
  let button0;
  let t4;
  let td4;
  let button1;
  let t6;
  let if_block_anchor;
  let mounted;
  let dispose;
  let each_value_77 = ensure_array_like(
    /*antGrpObjs*/
    ctx[671]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_77.length; i += 1) {
    each_blocks[i] = create_each_block_77(get_each_context_77(ctx, each_value_77, i));
  }
  let if_block = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_102()
  );
  return {
    c() {
      td0 = element("td");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      t2 = space();
      td3 = element("td");
      button0 = element("button");
      button0.textContent = "Save";
      t4 = space();
      td4 = element("td");
      button1 = element("button");
      button1.textContent = "Cancel";
      t6 = space();
      if (if_block) if_block.c();
      if_block_anchor = empty();
      attr(select, "class", "select");
      if (
        /*smBndAddRef*/
        ctx[35][
          /*activeSerial*/
          ctx[0]
        ].dest_id === void 0
      ) add_render_callback(() => (
        /*select_change_handler_3*/
        ctx[418].call(select)
      ));
      attr(td0, "colspan", "3");
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*smBndAddRef*/
        ctx[35][
          /*activeSerial*/
          ctx[0]
        ].dest_id,
        true
      );
      insert(target, t0, anchor);
      insert(target, td1, anchor);
      insert(target, t1, anchor);
      insert(target, td2, anchor);
      insert(target, t2, anchor);
      insert(target, td3, anchor);
      append(td3, button0);
      insert(target, t4, anchor);
      insert(target, td4, anchor);
      append(td4, button1);
      insert(target, t6, anchor);
      if (if_block) if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
      if (!mounted) {
        dispose = [
          listen(
            select,
            "change",
            /*select_change_handler_3*/
            ctx[418]
          ),
          listen(
            button0,
            "click",
            /*click_handler_39*/
            ctx[419]
          ),
          listen(
            button1,
            "click",
            /*click_handler_40*/
            ctx[420]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntsAndGroups*/
      8192) {
        each_value_77 = ensure_array_like(
          /*antGrpObjs*/
          ctx2[671]
        );
        let i;
        for (i = 0; i < each_value_77.length; i += 1) {
          const child_ctx = get_each_context_77(ctx2, each_value_77, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_77(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_77.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddRef, activeAntsAndGroups*/
      8208) {
        select_option(
          select,
          /*smBndAddRef*/
          ctx2[35][
            /*activeSerial*/
            ctx2[0]
          ].dest_id
        );
      }
      if (
        /*seqOuts*/
        ctx2[669].length > 0
      ) {
        if (if_block) ;
        else {
          if_block = create_if_block_102();
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t0);
        detach(td1);
        detach(t1);
        detach(td2);
        detach(t2);
        detach(td3);
        detach(t4);
        detach(td4);
        detach(t6);
        detach(if_block_anchor);
      }
      destroy_each(each_blocks, detaching);
      if (if_block) if_block.d(detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_105(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_104(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_103(ctx) {
  let button;
  let mounted;
  let dispose;
  function click_handler_41() {
    return (
      /*click_handler_41*/
      ctx[421](
        /*bnd*/
        ctx[672]
      )
    );
  }
  return {
    c() {
      button = element("button");
      button.textContent = "Add";
      attr(button, "class", "btn");
      attr(button, "type", "button");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      if (!mounted) {
        dispose = listen(button, "click", click_handler_41);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
    },
    d(detaching) {
      if (detaching) {
        detach(button);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_77(ctx) {
  let option;
  let t0_value = (
    /*obj*/
    ctx[644].type === 0 ? "ANT" : (
      /*obj*/
      ctx[644].type === 1 && /*obj*/
      ctx[644].virtual_rotator ? "VIR" : "GRP"
    )
  );
  let t0;
  let t1;
  let t2_value = (
    /*obj*/
    (ctx[644].display || /*obj*/
    ctx[644].label || `#${/*obj*/
    ctx[644].id}`) + ""
  );
  let t2;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t0 = text(t0_value);
      t1 = text(": ");
      t2 = text(t2_value);
      option.__value = option_value_value = /*obj*/
      ctx[644].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t0);
      append(option, t1);
      append(option, t2);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntsAndGroups*/
      8192 && t0_value !== (t0_value = /*obj*/
      ctx2[644].type === 0 ? "ANT" : (
        /*obj*/
        ctx2[644].type === 1 && /*obj*/
        ctx2[644].virtual_rotator ? "VIR" : "GRP"
      ))) set_data(t0, t0_value);
      if (dirty[1] & /*activeAntsAndGroups*/
      8192 && t2_value !== (t2_value = /*obj*/
      (ctx2[644].display || /*obj*/
      ctx2[644].label || `#${/*obj*/
      ctx2[644].id}`) + "")) set_data(t2, t2_value);
      if (dirty[1] & /*activeAntsAndGroups*/
      8192 && option_value_value !== (option_value_value = /*obj*/
      ctx2[644].id)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_102(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_76(key_1, ctx) {
  let tr0;
  let t0;
  let td0;
  let t1_value = (
    /*bnd*/
    ctx[672].id + ""
  );
  let t1;
  let t2;
  let t3;
  let each_blocks = [];
  let each_1_lookup = /* @__PURE__ */ new Map();
  let t4;
  let tr1;
  let td1;
  let t5;
  let td2;
  let t6;
  let t7;
  function select_block_type_17(ctx2, dirty) {
    if (
      /*bndMode*/
      ctx2[666] === "edit"
    ) return create_if_block_116;
    return create_else_block_21;
  }
  let current_block_type = select_block_type_17(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_18(ctx2, dirty) {
    if (
      /*bndMode*/
      ctx2[666] === "edit"
    ) return create_if_block_111;
    return create_else_block_20;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_111) return get_if_ctx_10(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_18(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  let each_value_78 = ensure_array_like(
    /*bnd*/
    ctx[672].refs || []
  );
  const get_key = (ctx2) => (
    /*ref*/
    ctx2[647].id
  );
  for (let i = 0; i < each_value_78.length; i += 1) {
    let child_ctx = get_each_context_78(ctx, each_value_78, i);
    let key = get_key(child_ctx);
    each_1_lookup.set(key, each_blocks[i] = create_each_block_78(key, child_ctx));
  }
  function select_block_type_21(ctx2, dirty) {
    if (
      /*bndAddRefState*/
      ctx2[667] && /*bndAddRefState*/
      ctx2[667].bndId === /*bnd*/
      ctx2[672].id
    ) return create_if_block_101;
    return create_else_block_17;
  }
  let current_block_type_2 = select_block_type_21(ctx);
  let if_block2 = current_block_type_2(ctx);
  return {
    key: key_1,
    first: null,
    c() {
      tr0 = element("tr");
      if_block0.c();
      t0 = space();
      td0 = element("td");
      t1 = text(t1_value);
      t2 = space();
      if_block1.c();
      t3 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t4 = space();
      tr1 = element("tr");
      td1 = element("td");
      t5 = space();
      td2 = element("td");
      t6 = space();
      if_block2.c();
      t7 = space();
      attr(tr0, "class", "vr-main-row");
      this.first = tr0;
    },
    m(target, anchor) {
      insert(target, tr0, anchor);
      if_block0.m(tr0, null);
      append(tr0, t0);
      append(tr0, td0);
      append(td0, t1);
      append(tr0, t2);
      if_block1.m(tr0, null);
      insert(target, t3, anchor);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, t4, anchor);
      insert(target, tr1, anchor);
      append(tr1, td1);
      append(tr1, t5);
      append(tr1, td2);
      append(tr1, t6);
      if_block2.m(tr1, null);
      append(tr1, t7);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_17(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr0, t0);
        }
      }
      if (dirty[1] & /*activeBnds*/
      131072 && t1_value !== (t1_value = /*bnd*/
      ctx[672].id + "")) set_data(t1, t1_value);
      if (current_block_type_1 === (current_block_type_1 = select_block_type_18(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr0, null);
        }
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeSeqOutputs, activeBpfOutputs, activeAntOutputs, activeAllObjs, activeBnds, smBndEditForms, activeAntsAndGroups, smBndMode, smBndSelected*/
      516109 | dirty[4] & /*smObjTypedDisplay*/
      16777216 | dirty[5] & /*smBndToggleSelect*/
      1) {
        each_value_78 = ensure_array_like(
          /*bnd*/
          ctx[672].refs || []
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx, each_value_78, each_1_lookup, t4.parentNode, destroy_block, create_each_block_78, t4, get_each_context_78);
      }
      if (current_block_type_2 === (current_block_type_2 = select_block_type_21(ctx)) && if_block2) {
        if_block2.p(ctx, dirty);
      } else {
        if_block2.d(1);
        if_block2 = current_block_type_2(ctx);
        if (if_block2) {
          if_block2.c();
          if_block2.m(tr1, t7);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr0);
        detach(t3);
        detach(t4);
        detach(tr1);
      }
      if_block0.d();
      if_block1.d();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d(detaching);
      }
      if_block2.d();
    }
  };
}
function create_if_block_98(ctx) {
  let tr;
  let td0;
  let t0;
  let td1;
  let t1;
  let td2;
  let input0;
  let t2;
  let td3;
  let input1;
  let t3;
  let td4;
  let input2;
  let t4;
  let td5;
  let input3;
  let t5;
  let td6;
  let input4;
  let t6;
  let td7;
  let input5;
  let t7;
  let td8;
  let t8;
  let t9;
  let mounted;
  let dispose;
  let if_block0 = (
    /*bpfOuts*/
    ctx[668].length > 0 && create_if_block_100()
  );
  let if_block1 = (
    /*seqOuts*/
    ctx[669].length > 0 && create_if_block_99()
  );
  return {
    c() {
      tr = element("tr");
      td0 = element("td");
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      input0 = element("input");
      t2 = space();
      td3 = element("td");
      input1 = element("input");
      t3 = space();
      td4 = element("td");
      input2 = element("input");
      t4 = space();
      td5 = element("td");
      input3 = element("input");
      t5 = space();
      td6 = element("td");
      input4 = element("input");
      t6 = space();
      td7 = element("td");
      input5 = element("input");
      t7 = space();
      td8 = element("td");
      t8 = space();
      if (if_block0) if_block0.c();
      t9 = space();
      if (if_block1) if_block1.c();
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "10");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      set_style(input1, "width", "90px");
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      set_style(input2, "width", "90px");
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      set_style(input3, "width", "40px");
      attr(input4, "type", "checkbox");
      attr(input5, "type", "checkbox");
      attr(tr, "class", "add-row");
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      append(tr, td0);
      append(tr, t0);
      append(tr, td1);
      append(tr, t1);
      append(tr, td2);
      append(td2, input0);
      set_input_value(
        input0,
        /*smBndAddForm*/
        ctx[32][
          /*activeSerial*/
          ctx[0]
        ].display
      );
      append(tr, t2);
      append(tr, td3);
      append(td3, input1);
      set_input_value(
        input1,
        /*smBndAddForm*/
        ctx[32][
          /*activeSerial*/
          ctx[0]
        ].low_freq
      );
      append(tr, t3);
      append(tr, td4);
      append(td4, input2);
      set_input_value(
        input2,
        /*smBndAddForm*/
        ctx[32][
          /*activeSerial*/
          ctx[0]
        ].high_freq
      );
      append(tr, t4);
      append(tr, td5);
      append(td5, input3);
      set_input_value(
        input3,
        /*smBndAddForm*/
        ctx[32][
          /*activeSerial*/
          ctx[0]
        ].bcd_code
      );
      append(tr, t5);
      append(tr, td6);
      append(td6, input4);
      input4.checked = /*smBndAddForm*/
      ctx[32][
        /*activeSerial*/
        ctx[0]
      ].pa_power;
      append(tr, t6);
      append(tr, td7);
      append(td7, input5);
      input5.checked = /*smBndAddForm*/
      ctx[32][
        /*activeSerial*/
        ctx[0]
      ].keyout;
      append(tr, t7);
      append(tr, td8);
      append(tr, t8);
      if (if_block0) if_block0.m(tr, null);
      append(tr, t9);
      if (if_block1) if_block1.m(tr, null);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler_4*/
            ctx[422]
          ),
          listen(
            input1,
            "input",
            /*input1_input_handler_4*/
            ctx[423]
          ),
          listen(
            input2,
            "input",
            /*input2_input_handler_1*/
            ctx[424]
          ),
          listen(
            input3,
            "input",
            /*input3_input_handler_1*/
            ctx[425]
          ),
          listen(
            input4,
            "change",
            /*input4_change_handler*/
            ctx[426]
          ),
          listen(
            input5,
            "change",
            /*input5_change_handler*/
            ctx[427]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddForm, activeAntsAndGroups*/
      8194 && input0.value !== /*smBndAddForm*/
      ctx2[32][
        /*activeSerial*/
        ctx2[0]
      ].display) {
        set_input_value(
          input0,
          /*smBndAddForm*/
          ctx2[32][
            /*activeSerial*/
            ctx2[0]
          ].display
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddForm, activeAntsAndGroups*/
      8194 && to_number(input1.value) !== /*smBndAddForm*/
      ctx2[32][
        /*activeSerial*/
        ctx2[0]
      ].low_freq) {
        set_input_value(
          input1,
          /*smBndAddForm*/
          ctx2[32][
            /*activeSerial*/
            ctx2[0]
          ].low_freq
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddForm, activeAntsAndGroups*/
      8194 && to_number(input2.value) !== /*smBndAddForm*/
      ctx2[32][
        /*activeSerial*/
        ctx2[0]
      ].high_freq) {
        set_input_value(
          input2,
          /*smBndAddForm*/
          ctx2[32][
            /*activeSerial*/
            ctx2[0]
          ].high_freq
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddForm, activeAntsAndGroups*/
      8194 && to_number(input3.value) !== /*smBndAddForm*/
      ctx2[32][
        /*activeSerial*/
        ctx2[0]
      ].bcd_code) {
        set_input_value(
          input3,
          /*smBndAddForm*/
          ctx2[32][
            /*activeSerial*/
            ctx2[0]
          ].bcd_code
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddForm, activeAntsAndGroups*/
      8194) {
        input4.checked = /*smBndAddForm*/
        ctx2[32][
          /*activeSerial*/
          ctx2[0]
        ].pa_power;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*smBndAddForm, activeAntsAndGroups*/
      8194) {
        input5.checked = /*smBndAddForm*/
        ctx2[32][
          /*activeSerial*/
          ctx2[0]
        ].keyout;
      }
      if (
        /*bpfOuts*/
        ctx2[668].length > 0
      ) {
        if (if_block0) ;
        else {
          if_block0 = create_if_block_100();
          if_block0.c();
          if_block0.m(tr, t9);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*seqOuts*/
        ctx2[669].length > 0
      ) {
        if (if_block1) ;
        else {
          if_block1 = create_if_block_99();
          if_block1.c();
          if_block1.m(tr, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_100(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_99(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_16(ctx) {
  let button0;
  let t1;
  let button1;
  let t3;
  let button2;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Add";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Edit";
      t3 = space();
      button2 = element("button");
      button2.textContent = "Remove";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
      attr(button2, "class", "btn");
      attr(button2, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      insert(target, t3, anchor);
      insert(target, button2, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_46*/
            ctx[432]
          ),
          listen(
            button1,
            "click",
            /*click_handler_47*/
            ctx[433]
          ),
          listen(
            button2,
            "click",
            /*click_handler_48*/
            ctx[434]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
        detach(t3);
        detach(button2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_97(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_44*/
            ctx[430]
          ),
          listen(
            button1,
            "click",
            /*click_handler_45*/
            ctx[431]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_96(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_42*/
            ctx[428]
          ),
          listen(
            button1,
            "click",
            /*click_handler_43*/
            ctx[429]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_95(ctx) {
  return { c: noop, m: noop, p: noop, d: noop };
}
function create_if_block_94(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_else_block_15(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_128() {
    return (
      /*change_handler_128*/
      ctx[387](
        /*grp*/
        ctx[656]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smGrpSelected*/
      (ctx[29][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*grp*/
        ctx[656].id
      );
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_128);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smGrpSelected, activeSerial*/
      536870913 | dirty[1] & /*activeGrps, activeAntsAndGroups, activeAllObjs*/
      794624 && input_checked_value !== (input_checked_value = /*smGrpSelected*/
      (ctx[29][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*grp*/
        ctx[656].id
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_92(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_14(ctx) {
  let td0;
  let t0_value = (
    /*grp*/
    (ctx[656].label || "") + ""
  );
  let t0;
  let t1;
  let td1;
  let t2_value = (
    /*grp*/
    (ctx[656].display || "") + ""
  );
  let t2;
  return {
    c() {
      td0 = element("td");
      t0 = text(t0_value);
      t1 = space();
      td1 = element("td");
      t2 = text(t2_value);
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, t0);
      insert(target, t1, anchor);
      insert(target, td1, anchor);
      append(td1, t2);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeGrps*/
      524288 && t0_value !== (t0_value = /*grp*/
      (ctx2[656].label || "") + "")) set_data(t0, t0_value);
      if (dirty[1] & /*activeGrps*/
      524288 && t2_value !== (t2_value = /*grp*/
      (ctx2[656].display || "") + "")) set_data(t2, t2_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t1);
        detach(td1);
      }
    }
  };
}
function create_if_block_91(ctx) {
  let td0;
  let input0;
  let input0_value_value;
  let t;
  let td1;
  let input1;
  let input1_value_value;
  let mounted;
  let dispose;
  function input_handler_45(...args) {
    return (
      /*input_handler_45*/
      ctx[388](
        /*grp*/
        ctx[656],
        ...args
      )
    );
  }
  function input_handler_46(...args) {
    return (
      /*input_handler_46*/
      ctx[389](
        /*grp*/
        ctx[656],
        ...args
      )
    );
  }
  return {
    c() {
      td0 = element("td");
      input0 = element("input");
      t = space();
      td1 = element("td");
      input1 = element("input");
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "5");
      input0.value = input0_value_value = /*ef*/
      ctx[630].label;
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      attr(input1, "size", "10");
      input1.value = input1_value_value = /*ef*/
      ctx[630].display;
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, input0);
      insert(target, t, anchor);
      insert(target, td1, anchor);
      append(td1, input1);
      if (!mounted) {
        dispose = [
          listen(input0, "input", input_handler_45),
          listen(input1, "input", input_handler_46)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smGrpEditForms, activeSerial*/
      268435457 | dirty[1] & /*activeGrps, activeAllObjs, activeAntsAndGroups*/
      794624 && input0_value_value !== (input0_value_value = /*ef*/
      ctx[630].label) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*smGrpEditForms, activeSerial*/
      268435457 | dirty[1] & /*activeGrps, activeAllObjs, activeAntsAndGroups*/
      794624 && input1_value_value !== (input1_value_value = /*ef*/
      ctx[630].display) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t);
        detach(td1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_90(ctx) {
  let button;
  let mounted;
  let dispose;
  function click_handler_29() {
    return (
      /*click_handler_29*/
      ctx[390](
        /*grp*/
        ctx[656]
      )
    );
  }
  return {
    c() {
      button = element("button");
      button.textContent = "Add Antenna";
      attr(button, "class", "btn");
      attr(button, "type", "button");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      if (!mounted) {
        dispose = listen(button, "click", click_handler_29);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
    },
    d(detaching) {
      if (detaching) {
        detach(button);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_else_block_13(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_129() {
    return (
      /*change_handler_129*/
      ctx[391](
        /*grp*/
        ctx[656],
        /*ref*/
        ctx[647]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smGrpSelected*/
      (ctx[29][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(`${/*grp*/
      ctx[656].id}:${/*ref*/
      ctx[647].id}`);
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_129);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smGrpSelected, activeSerial*/
      536870913 | dirty[1] & /*activeGrps, activeAntsAndGroups, activeAllObjs*/
      794624 && input_checked_value !== (input_checked_value = /*smGrpSelected*/
      (ctx[29][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(`${/*grp*/
      ctx[656].id}:${/*ref*/
      ctx[647].id}`))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_89(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_12(ctx) {
  let td;
  let t_value = (
    /*smObjDisplay*/
    ctx[126](
      /*activeSerial*/
      ctx[0],
      /*ref*/
      ctx[647].dest_id
    ) + ""
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeGrps*/
      524288 && t_value !== (t_value = /*smObjDisplay*/
      ctx2[126](
        /*activeSerial*/
        ctx2[0],
        /*ref*/
        ctx2[647].dest_id
      ) + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_if_block_88(ctx) {
  let td;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  let each_value_75 = ensure_array_like(
    /*allObjs*/
    ctx[640]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_75.length; i += 1) {
    each_blocks[i] = create_each_block_75(get_each_context_75(ctx, each_value_75, i));
  }
  function change_handler_130(...args) {
    return (
      /*change_handler_130*/
      ctx[392](
        /*grp*/
        ctx[656],
        /*ref*/
        ctx[647],
        ...args
      )
    );
  }
  return {
    c() {
      td = element("td");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(select, "class", "select");
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*rf*/
        ctx[650].dest_id
      );
      if (!mounted) {
        dispose = listen(select, "change", change_handler_130);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[1] & /*activeAllObjs*/
      262144) {
        each_value_75 = ensure_array_like(
          /*allObjs*/
          ctx[640]
        );
        let i;
        for (i = 0; i < each_value_75.length; i += 1) {
          const child_ctx = get_each_context_75(ctx, each_value_75, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_75(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_75.length;
      }
      if (dirty[0] & /*smGrpEditForms, activeSerial*/
      268435457 | dirty[1] & /*activeGrps, activeAllObjs, activeAntsAndGroups*/
      794624 && select_value_value !== (select_value_value = /*rf*/
      ctx[650].dest_id)) {
        select_option(
          select,
          /*rf*/
          ctx[650].dest_id
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_75(ctx) {
  let option;
  let t_value = (
    /*obj*/
    (ctx[644].display || /*obj*/
    ctx[644].label || `#${/*obj*/
    ctx[644].id}`) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*obj*/
      ctx[644].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAllObjs*/
      262144 && t_value !== (t_value = /*obj*/
      (ctx2[644].display || /*obj*/
      ctx2[644].label || `#${/*obj*/
      ctx2[644].id}`) + "")) set_data(t, t_value);
      if (dirty[1] & /*activeAllObjs*/
      262144 && option_value_value !== (option_value_value = /*obj*/
      ctx2[644].id)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_74(key_1, ctx) {
  let tr;
  let t0;
  let td0;
  let t1;
  let td1;
  let t2;
  let td2;
  let t3;
  let td3;
  let t4_value = (
    /*smObjRxOnly*/
    ctx[127](
      /*activeSerial*/
      ctx[0],
      /*ref*/
      ctx[647].dest_id
    ) + ""
  );
  let t4;
  let t5;
  let t6;
  function select_block_type_14(ctx2, dirty) {
    if (
      /*grpMode*/
      ctx2[654] === "edit"
    ) return create_if_block_89;
    return create_else_block_13;
  }
  let current_block_type = select_block_type_14(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_15(ctx2, dirty) {
    if (
      /*grpMode*/
      ctx2[654] === "edit"
    ) return create_if_block_88;
    return create_else_block_12;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_88) return get_if_ctx_6(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_15(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  return {
    key: key_1,
    first: null,
    c() {
      tr = element("tr");
      if_block0.c();
      t0 = space();
      td0 = element("td");
      t1 = space();
      td1 = element("td");
      t2 = space();
      td2 = element("td");
      t3 = space();
      td3 = element("td");
      t4 = text(t4_value);
      t5 = space();
      if_block1.c();
      t6 = space();
      this.first = tr;
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      if_block0.m(tr, null);
      append(tr, t0);
      append(tr, td0);
      append(tr, t1);
      append(tr, td1);
      append(tr, t2);
      append(tr, td2);
      append(tr, t3);
      append(tr, td3);
      append(td3, t4);
      append(tr, t5);
      if_block1.m(tr, null);
      append(tr, t6);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_14(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr, t0);
        }
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeGrps*/
      524288 && t4_value !== (t4_value = /*smObjRxOnly*/
      ctx[127](
        /*activeSerial*/
        ctx[0],
        /*ref*/
        ctx[647].dest_id
      ) + "")) set_data(t4, t4_value);
      if (current_block_type_1 === (current_block_type_1 = select_block_type_15(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr, t6);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      if_block0.d();
      if_block1.d();
    }
  };
}
function create_if_block_87(ctx) {
  let tr;
  let td0;
  let t0;
  let td1;
  let t1;
  let td2;
  let t2;
  let td3;
  let t3;
  let td4;
  let t4;
  let td5;
  let select;
  let t5;
  let mounted;
  let dispose;
  let each_value_73 = ensure_array_like(
    /*allObjs*/
    ctx[640]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_73.length; i += 1) {
    each_blocks[i] = create_each_block_73(get_each_context_73(ctx, each_value_73, i));
  }
  return {
    c() {
      tr = element("tr");
      td0 = element("td");
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      t2 = space();
      td3 = element("td");
      t3 = space();
      td4 = element("td");
      t4 = space();
      td5 = element("td");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t5 = space();
      attr(select, "class", "select");
      if (
        /*smGrpAddAnt*/
        ctx[30][
          /*activeSerial*/
          ctx[0]
        ].dest_id === void 0
      ) add_render_callback(() => (
        /*select_change_handler_2*/
        ctx[393].call(select)
      ));
      attr(tr, "class", "add-row");
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      append(tr, td0);
      append(tr, t0);
      append(tr, td1);
      append(tr, t1);
      append(tr, td2);
      append(tr, t2);
      append(tr, td3);
      append(tr, t3);
      append(tr, td4);
      append(tr, t4);
      append(tr, td5);
      append(td5, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*smGrpAddAnt*/
        ctx[30][
          /*activeSerial*/
          ctx[0]
        ].dest_id,
        true
      );
      append(tr, t5);
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*select_change_handler_2*/
          ctx[393]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAllObjs*/
      262144) {
        each_value_73 = ensure_array_like(
          /*allObjs*/
          ctx2[640]
        );
        let i;
        for (i = 0; i < each_value_73.length; i += 1) {
          const child_ctx = get_each_context_73(ctx2, each_value_73, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_73(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_73.length;
      }
      if (dirty[0] & /*smGrpAddAnt, activeSerial*/
      1073741825 | dirty[1] & /*activeAllObjs, activeAntsAndGroups*/
      270336) {
        select_option(
          select,
          /*smGrpAddAnt*/
          ctx2[30][
            /*activeSerial*/
            ctx2[0]
          ].dest_id
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_73(ctx) {
  let option;
  let t_value = (
    /*obj*/
    (ctx[644].display || /*obj*/
    ctx[644].label || `#${/*obj*/
    ctx[644].id}`) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*obj*/
      ctx[644].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAllObjs*/
      262144 && t_value !== (t_value = /*obj*/
      (ctx2[644].display || /*obj*/
      ctx2[644].label || `#${/*obj*/
      ctx2[644].id}`) + "")) set_data(t, t_value);
      if (dirty[1] & /*activeAllObjs*/
      262144 && option_value_value !== (option_value_value = /*obj*/
      ctx2[644].id)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_72(key_1, ctx) {
  let tr;
  let t0;
  let td0;
  let t1_value = (
    /*grp*/
    ctx[656].id + ""
  );
  let t1;
  let t2;
  let t3;
  let td1;
  let t4;
  let td2;
  let t5;
  let each_blocks = [];
  let each_1_lookup = /* @__PURE__ */ new Map();
  let t6;
  let if_block3_anchor;
  function select_block_type_12(ctx2, dirty) {
    if (
      /*grpMode*/
      ctx2[654] === "edit"
    ) return create_if_block_92;
    return create_else_block_15;
  }
  let current_block_type = select_block_type_12(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_13(ctx2, dirty) {
    if (
      /*grpMode*/
      ctx2[654] === "edit"
    ) return create_if_block_91;
    return create_else_block_14;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_91) return get_if_ctx_7(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_13(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  let if_block2 = !/*grpMode*/
  ctx[654] && !/*grpAddAntState*/
  ctx[655] && create_if_block_90(ctx);
  let each_value_74 = ensure_array_like(
    /*grp*/
    ctx[656].refs || []
  );
  const get_key = (ctx2) => (
    /*ref*/
    ctx2[647].id
  );
  for (let i = 0; i < each_value_74.length; i += 1) {
    let child_ctx = get_each_context_74(ctx, each_value_74, i);
    let key = get_key(child_ctx);
    each_1_lookup.set(key, each_blocks[i] = create_each_block_74(key, child_ctx));
  }
  let if_block3 = (
    /*grpAddAntState*/
    ctx[655] && /*grpAddAntState*/
    ctx[655].grpId === /*grp*/
    ctx[656].id && create_if_block_87(ctx)
  );
  return {
    key: key_1,
    first: null,
    c() {
      tr = element("tr");
      if_block0.c();
      t0 = space();
      td0 = element("td");
      t1 = text(t1_value);
      t2 = space();
      if_block1.c();
      t3 = space();
      td1 = element("td");
      t4 = space();
      td2 = element("td");
      if (if_block2) if_block2.c();
      t5 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t6 = space();
      if (if_block3) if_block3.c();
      if_block3_anchor = empty();
      attr(tr, "class", "vr-main-row");
      this.first = tr;
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      if_block0.m(tr, null);
      append(tr, t0);
      append(tr, td0);
      append(td0, t1);
      append(tr, t2);
      if_block1.m(tr, null);
      append(tr, t3);
      append(tr, td1);
      append(tr, t4);
      append(tr, td2);
      if (if_block2) if_block2.m(td2, null);
      insert(target, t5, anchor);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, t6, anchor);
      if (if_block3) if_block3.m(target, anchor);
      insert(target, if_block3_anchor, anchor);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_12(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr, t0);
        }
      }
      if (dirty[1] & /*activeGrps*/
      524288 && t1_value !== (t1_value = /*grp*/
      ctx[656].id + "")) set_data(t1, t1_value);
      if (current_block_type_1 === (current_block_type_1 = select_block_type_13(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr, t3);
        }
      }
      if (!/*grpMode*/
      ctx[654] && !/*grpAddAntState*/
      ctx[655]) {
        if (if_block2) {
          if_block2.p(ctx, dirty);
        } else {
          if_block2 = create_if_block_90(ctx);
          if_block2.c();
          if_block2.m(td2, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (dirty[0] & /*smGrpEditForms, activeSerial, smGrpMode, smGrpSelected*/
      872415233 | dirty[1] & /*activeGrps, activeAllObjs*/
      786432 | dirty[4] & /*smObjDisplay, smObjRxOnly, smGrpToggleSelect*/
      1048588) {
        each_value_74 = ensure_array_like(
          /*grp*/
          ctx[656].refs || []
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx, each_value_74, each_1_lookup, t6.parentNode, destroy_block, create_each_block_74, t6, get_each_context_74);
      }
      if (
        /*grpAddAntState*/
        ctx[655] && /*grpAddAntState*/
        ctx[655].grpId === /*grp*/
        ctx[656].id
      ) {
        if (if_block3) {
          if_block3.p(ctx, dirty);
        } else {
          if_block3 = create_if_block_87(ctx);
          if_block3.c();
          if_block3.m(if_block3_anchor.parentNode, if_block3_anchor);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
        detach(t5);
        detach(t6);
        detach(if_block3_anchor);
      }
      if_block0.d();
      if_block1.d();
      if (if_block2) if_block2.d();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d(detaching);
      }
      if (if_block3) if_block3.d(detaching);
    }
  };
}
function create_if_block_86(ctx) {
  let tr;
  let td0;
  let t0;
  let td1;
  let t1;
  let td2;
  let input0;
  let t2;
  let td3;
  let input1;
  let t3;
  let td4;
  let t4;
  let td5;
  let mounted;
  let dispose;
  return {
    c() {
      tr = element("tr");
      td0 = element("td");
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      input0 = element("input");
      t2 = space();
      td3 = element("td");
      input1 = element("input");
      t3 = space();
      td4 = element("td");
      t4 = space();
      td5 = element("td");
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "5");
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      attr(input1, "size", "10");
      attr(tr, "class", "add-row");
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      append(tr, td0);
      append(tr, t0);
      append(tr, td1);
      append(tr, t1);
      append(tr, td2);
      append(td2, input0);
      set_input_value(
        input0,
        /*smGrpAddForm*/
        ctx[27][
          /*activeSerial*/
          ctx[0]
        ].label
      );
      append(tr, t2);
      append(tr, td3);
      append(td3, input1);
      set_input_value(
        input1,
        /*smGrpAddForm*/
        ctx[27][
          /*activeSerial*/
          ctx[0]
        ].display
      );
      append(tr, t3);
      append(tr, td4);
      append(tr, t4);
      append(tr, td5);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler_3*/
            ctx[394]
          ),
          listen(
            input1,
            "input",
            /*input1_input_handler_3*/
            ctx[395]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smGrpAddForm, activeSerial*/
      134217729 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0.value !== /*smGrpAddForm*/
      ctx2[27][
        /*activeSerial*/
        ctx2[0]
      ].label) {
        set_input_value(
          input0,
          /*smGrpAddForm*/
          ctx2[27][
            /*activeSerial*/
            ctx2[0]
          ].label
        );
      }
      if (dirty[0] & /*smGrpAddForm, activeSerial*/
      134217729 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1.value !== /*smGrpAddForm*/
      ctx2[27][
        /*activeSerial*/
        ctx2[0]
      ].display) {
        set_input_value(
          input1,
          /*smGrpAddForm*/
          ctx2[27][
            /*activeSerial*/
            ctx2[0]
          ].display
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_else_block_11(ctx) {
  let button0;
  let t1;
  let button1;
  let t3;
  let button2;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Add";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Edit";
      t3 = space();
      button2 = element("button");
      button2.textContent = "Remove";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
      attr(button2, "class", "btn");
      attr(button2, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      insert(target, t3, anchor);
      insert(target, button2, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_36*/
            ctx[402]
          ),
          listen(
            button1,
            "click",
            /*click_handler_37*/
            ctx[403]
          ),
          listen(
            button2,
            "click",
            /*click_handler_38*/
            ctx[404]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
        detach(t3);
        detach(button2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_85(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_34*/
            ctx[400]
          ),
          listen(
            button1,
            "click",
            /*click_handler_35*/
            ctx[401]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_84(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_32*/
            ctx[398]
          ),
          listen(
            button1,
            "click",
            /*click_handler_33*/
            ctx[399]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_83(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_30*/
            ctx[396]
          ),
          listen(
            button1,
            "click",
            /*click_handler_31*/
            ctx[397]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_82(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_else_block_10(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_125() {
    return (
      /*change_handler_125*/
      ctx[365](
        /*vr*/
        ctx[641]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smVrSelected*/
      (ctx[24][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*vr*/
        ctx[641].id
      );
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_125);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smVrSelected, activeSerial*/
      16777217 | dirty[1] & /*activeVrs, activeAntsAndGroups, activeAllObjs*/
      1318912 && input_checked_value !== (input_checked_value = /*smVrSelected*/
      (ctx[24][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*vr*/
        ctx[641].id
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_80(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_9(ctx) {
  let td0;
  let t0_value = (
    /*vr*/
    (ctx[641].label || "") + ""
  );
  let t0;
  let t1;
  let td1;
  let t2_value = (
    /*vr*/
    (ctx[641].display || "") + ""
  );
  let t2;
  return {
    c() {
      td0 = element("td");
      t0 = text(t0_value);
      t1 = space();
      td1 = element("td");
      t2 = text(t2_value);
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, t0);
      insert(target, t1, anchor);
      insert(target, td1, anchor);
      append(td1, t2);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeVrs*/
      1048576 && t0_value !== (t0_value = /*vr*/
      (ctx2[641].label || "") + "")) set_data(t0, t0_value);
      if (dirty[1] & /*activeVrs*/
      1048576 && t2_value !== (t2_value = /*vr*/
      (ctx2[641].display || "") + "")) set_data(t2, t2_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t1);
        detach(td1);
      }
    }
  };
}
function create_if_block_79(ctx) {
  let td0;
  let input0;
  let input0_value_value;
  let t;
  let td1;
  let input1;
  let input1_value_value;
  let mounted;
  let dispose;
  function input_handler_41(...args) {
    return (
      /*input_handler_41*/
      ctx[366](
        /*vr*/
        ctx[641],
        ...args
      )
    );
  }
  function input_handler_42(...args) {
    return (
      /*input_handler_42*/
      ctx[367](
        /*vr*/
        ctx[641],
        ...args
      )
    );
  }
  return {
    c() {
      td0 = element("td");
      input0 = element("input");
      t = space();
      td1 = element("td");
      input1 = element("input");
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "5");
      input0.value = input0_value_value = /*ef*/
      ctx[630].label;
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      attr(input1, "size", "10");
      input1.value = input1_value_value = /*ef*/
      ctx[630].display;
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, input0);
      insert(target, t, anchor);
      insert(target, td1, anchor);
      append(td1, input1);
      if (!mounted) {
        dispose = [
          listen(input0, "input", input_handler_41),
          listen(input1, "input", input_handler_42)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smVrEditForms, activeSerial*/
      8388609 | dirty[1] & /*activeVrs, activeAllObjs, activeAntsAndGroups*/
      1318912 && input0_value_value !== (input0_value_value = /*ef*/
      ctx[630].label) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*smVrEditForms, activeSerial*/
      8388609 | dirty[1] & /*activeVrs, activeAllObjs, activeAntsAndGroups*/
      1318912 && input1_value_value !== (input1_value_value = /*ef*/
      ctx[630].display) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t);
        detach(td1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_78(ctx) {
  let button;
  let mounted;
  let dispose;
  function click_handler_19() {
    return (
      /*click_handler_19*/
      ctx[368](
        /*vr*/
        ctx[641]
      )
    );
  }
  return {
    c() {
      button = element("button");
      button.textContent = "Add Antenna";
      attr(button, "class", "btn");
      attr(button, "type", "button");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      if (!mounted) {
        dispose = listen(button, "click", click_handler_19);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
    },
    d(detaching) {
      if (detaching) {
        detach(button);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_else_block_8(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_126() {
    return (
      /*change_handler_126*/
      ctx[369](
        /*vr*/
        ctx[641],
        /*ref*/
        ctx[647]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smVrSelected*/
      (ctx[24][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(`${/*vr*/
      ctx[641].id}:${/*ref*/
      ctx[647].id}`);
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_126);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smVrSelected, activeSerial*/
      16777217 | dirty[1] & /*activeVrs, activeAntsAndGroups, activeAllObjs*/
      1318912 && input_checked_value !== (input_checked_value = /*smVrSelected*/
      (ctx[24][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(`${/*vr*/
      ctx[641].id}:${/*ref*/
      ctx[647].id}`))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_77(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_7(ctx) {
  let td0;
  let t0_value = (
    /*ref*/
    (ctx[647].min_azimuth || 0) + ""
  );
  let t0;
  let t1;
  let td1;
  let t2_value = (
    /*ref*/
    (ctx[647].max_azimuth || 0) + ""
  );
  let t2;
  let t3;
  let td2;
  let t4_value = (
    /*smObjDisplay*/
    ctx[126](
      /*activeSerial*/
      ctx[0],
      /*ref*/
      ctx[647].dest_id
    ) + ""
  );
  let t4;
  return {
    c() {
      td0 = element("td");
      t0 = text(t0_value);
      t1 = space();
      td1 = element("td");
      t2 = text(t2_value);
      t3 = space();
      td2 = element("td");
      t4 = text(t4_value);
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, t0);
      insert(target, t1, anchor);
      insert(target, td1, anchor);
      append(td1, t2);
      insert(target, t3, anchor);
      insert(target, td2, anchor);
      append(td2, t4);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeVrs*/
      1048576 && t0_value !== (t0_value = /*ref*/
      (ctx2[647].min_azimuth || 0) + "")) set_data(t0, t0_value);
      if (dirty[1] & /*activeVrs*/
      1048576 && t2_value !== (t2_value = /*ref*/
      (ctx2[647].max_azimuth || 0) + "")) set_data(t2, t2_value);
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeVrs*/
      1048576 && t4_value !== (t4_value = /*smObjDisplay*/
      ctx2[126](
        /*activeSerial*/
        ctx2[0],
        /*ref*/
        ctx2[647].dest_id
      ) + "")) set_data(t4, t4_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t1);
        detach(td1);
        detach(t3);
        detach(td2);
      }
    }
  };
}
function create_if_block_76(ctx) {
  let td0;
  let input0;
  let input0_value_value;
  let t0;
  let td1;
  let input1;
  let input1_value_value;
  let t1;
  let td2;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  function input_handler_43(...args) {
    return (
      /*input_handler_43*/
      ctx[370](
        /*vr*/
        ctx[641],
        /*ref*/
        ctx[647],
        ...args
      )
    );
  }
  function input_handler_44(...args) {
    return (
      /*input_handler_44*/
      ctx[371](
        /*vr*/
        ctx[641],
        /*ref*/
        ctx[647],
        ...args
      )
    );
  }
  let each_value_71 = ensure_array_like(
    /*allObjs*/
    ctx[640]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_71.length; i += 1) {
    each_blocks[i] = create_each_block_71(get_each_context_71(ctx, each_value_71, i));
  }
  function change_handler_127(...args) {
    return (
      /*change_handler_127*/
      ctx[372](
        /*vr*/
        ctx[641],
        /*ref*/
        ctx[647],
        ...args
      )
    );
  }
  return {
    c() {
      td0 = element("td");
      input0 = element("input");
      t0 = space();
      td1 = element("td");
      input1 = element("input");
      t1 = space();
      td2 = element("td");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      set_style(input0, "width", "70px");
      input0.value = input0_value_value = /*rf*/
      ctx[650].min_azimuth;
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      set_style(input1, "width", "70px");
      input1.value = input1_value_value = /*rf*/
      ctx[650].max_azimuth;
      attr(select, "class", "select");
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, input0);
      insert(target, t0, anchor);
      insert(target, td1, anchor);
      append(td1, input1);
      insert(target, t1, anchor);
      insert(target, td2, anchor);
      append(td2, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*rf*/
        ctx[650].dest_id
      );
      if (!mounted) {
        dispose = [
          listen(input0, "input", input_handler_43),
          listen(input1, "input", input_handler_44),
          listen(select, "change", change_handler_127)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smVrEditForms, activeSerial*/
      8388609 | dirty[1] & /*activeVrs, activeAllObjs, activeAntsAndGroups*/
      1318912 && input0_value_value !== (input0_value_value = /*rf*/
      ctx[650].min_azimuth) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*smVrEditForms, activeSerial*/
      8388609 | dirty[1] & /*activeVrs, activeAllObjs, activeAntsAndGroups*/
      1318912 && input1_value_value !== (input1_value_value = /*rf*/
      ctx[650].max_azimuth) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[1] & /*activeAllObjs*/
      262144) {
        each_value_71 = ensure_array_like(
          /*allObjs*/
          ctx[640]
        );
        let i;
        for (i = 0; i < each_value_71.length; i += 1) {
          const child_ctx = get_each_context_71(ctx, each_value_71, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_71(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_71.length;
      }
      if (dirty[0] & /*smVrEditForms, activeSerial*/
      8388609 | dirty[1] & /*activeVrs, activeAllObjs, activeAntsAndGroups*/
      1318912 && select_value_value !== (select_value_value = /*rf*/
      ctx[650].dest_id)) {
        select_option(
          select,
          /*rf*/
          ctx[650].dest_id
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t0);
        detach(td1);
        detach(t1);
        detach(td2);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_71(ctx) {
  let option;
  let t_value = (
    /*obj*/
    (ctx[644].display || /*obj*/
    ctx[644].label || `#${/*obj*/
    ctx[644].id}`) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*obj*/
      ctx[644].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAllObjs*/
      262144 && t_value !== (t_value = /*obj*/
      (ctx2[644].display || /*obj*/
      ctx2[644].label || `#${/*obj*/
      ctx2[644].id}`) + "")) set_data(t, t_value);
      if (dirty[1] & /*activeAllObjs*/
      262144 && option_value_value !== (option_value_value = /*obj*/
      ctx2[644].id)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_70(key_1, ctx) {
  let tr;
  let t0;
  let td0;
  let t1;
  let td1;
  let t2;
  let td2;
  let t3;
  let td3;
  let t4_value = (
    /*smObjRxOnly*/
    ctx[127](
      /*activeSerial*/
      ctx[0],
      /*ref*/
      ctx[647].dest_id
    ) + ""
  );
  let t4;
  let t5;
  let t6;
  function select_block_type_9(ctx2, dirty) {
    if (
      /*vrMode*/
      ctx2[638] === "edit"
    ) return create_if_block_77;
    return create_else_block_8;
  }
  let current_block_type = select_block_type_9(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_10(ctx2, dirty) {
    if (
      /*vrMode*/
      ctx2[638] === "edit"
    ) return create_if_block_76;
    return create_else_block_7;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_76) return get_if_ctx_3(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_10(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  return {
    key: key_1,
    first: null,
    c() {
      tr = element("tr");
      if_block0.c();
      t0 = space();
      td0 = element("td");
      t1 = space();
      td1 = element("td");
      t2 = space();
      td2 = element("td");
      t3 = space();
      td3 = element("td");
      t4 = text(t4_value);
      t5 = space();
      if_block1.c();
      t6 = space();
      this.first = tr;
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      if_block0.m(tr, null);
      append(tr, t0);
      append(tr, td0);
      append(tr, t1);
      append(tr, td1);
      append(tr, t2);
      append(tr, td2);
      append(tr, t3);
      append(tr, td3);
      append(td3, t4);
      append(tr, t5);
      if_block1.m(tr, null);
      append(tr, t6);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_9(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr, t0);
        }
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeVrs*/
      1048576 && t4_value !== (t4_value = /*smObjRxOnly*/
      ctx[127](
        /*activeSerial*/
        ctx[0],
        /*ref*/
        ctx[647].dest_id
      ) + "")) set_data(t4, t4_value);
      if (current_block_type_1 === (current_block_type_1 = select_block_type_10(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr, t6);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      if_block0.d();
      if_block1.d();
    }
  };
}
function create_if_block_75(ctx) {
  let tr;
  let td0;
  let t0;
  let td1;
  let t1;
  let td2;
  let t2;
  let td3;
  let t3;
  let td4;
  let t4;
  let td5;
  let input0;
  let t5;
  let td6;
  let input1;
  let t6;
  let td7;
  let select;
  let t7;
  let mounted;
  let dispose;
  let each_value_69 = ensure_array_like(
    /*allObjs*/
    ctx[640]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_69.length; i += 1) {
    each_blocks[i] = create_each_block_69(get_each_context_69(ctx, each_value_69, i));
  }
  return {
    c() {
      tr = element("tr");
      td0 = element("td");
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      t2 = space();
      td3 = element("td");
      t3 = space();
      td4 = element("td");
      t4 = space();
      td5 = element("td");
      input0 = element("input");
      t5 = space();
      td6 = element("td");
      input1 = element("input");
      t6 = space();
      td7 = element("td");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t7 = space();
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      set_style(input0, "width", "70px");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      set_style(input1, "width", "70px");
      attr(select, "class", "select");
      if (
        /*smVrAddAnt*/
        ctx[25][
          /*activeSerial*/
          ctx[0]
        ].dest_id === void 0
      ) add_render_callback(() => (
        /*select_change_handler_1*/
        ctx[375].call(select)
      ));
      attr(tr, "class", "add-row");
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      append(tr, td0);
      append(tr, t0);
      append(tr, td1);
      append(tr, t1);
      append(tr, td2);
      append(tr, t2);
      append(tr, td3);
      append(tr, t3);
      append(tr, td4);
      append(tr, t4);
      append(tr, td5);
      append(td5, input0);
      set_input_value(
        input0,
        /*smVrAddAnt*/
        ctx[25][
          /*activeSerial*/
          ctx[0]
        ].min_azimuth
      );
      append(tr, t5);
      append(tr, td6);
      append(td6, input1);
      set_input_value(
        input1,
        /*smVrAddAnt*/
        ctx[25][
          /*activeSerial*/
          ctx[0]
        ].max_azimuth
      );
      append(tr, t6);
      append(tr, td7);
      append(td7, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*smVrAddAnt*/
        ctx[25][
          /*activeSerial*/
          ctx[0]
        ].dest_id,
        true
      );
      append(tr, t7);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler_1*/
            ctx[373]
          ),
          listen(
            input1,
            "input",
            /*input1_input_handler_1*/
            ctx[374]
          ),
          listen(
            select,
            "change",
            /*select_change_handler_1*/
            ctx[375]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smVrAddAnt, activeSerial*/
      33554433 | dirty[1] & /*activeAllObjs, activeAntsAndGroups*/
      270336 && to_number(input0.value) !== /*smVrAddAnt*/
      ctx2[25][
        /*activeSerial*/
        ctx2[0]
      ].min_azimuth) {
        set_input_value(
          input0,
          /*smVrAddAnt*/
          ctx2[25][
            /*activeSerial*/
            ctx2[0]
          ].min_azimuth
        );
      }
      if (dirty[0] & /*smVrAddAnt, activeSerial*/
      33554433 | dirty[1] & /*activeAllObjs, activeAntsAndGroups*/
      270336 && to_number(input1.value) !== /*smVrAddAnt*/
      ctx2[25][
        /*activeSerial*/
        ctx2[0]
      ].max_azimuth) {
        set_input_value(
          input1,
          /*smVrAddAnt*/
          ctx2[25][
            /*activeSerial*/
            ctx2[0]
          ].max_azimuth
        );
      }
      if (dirty[1] & /*activeAllObjs*/
      262144) {
        each_value_69 = ensure_array_like(
          /*allObjs*/
          ctx2[640]
        );
        let i;
        for (i = 0; i < each_value_69.length; i += 1) {
          const child_ctx = get_each_context_69(ctx2, each_value_69, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_69(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_69.length;
      }
      if (dirty[0] & /*smVrAddAnt, activeSerial*/
      33554433 | dirty[1] & /*activeAllObjs, activeAntsAndGroups*/
      270336) {
        select_option(
          select,
          /*smVrAddAnt*/
          ctx2[25][
            /*activeSerial*/
            ctx2[0]
          ].dest_id
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_69(ctx) {
  let option;
  let t_value = (
    /*obj*/
    (ctx[644].display || /*obj*/
    ctx[644].label || `#${/*obj*/
    ctx[644].id}`) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*obj*/
      ctx[644].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAllObjs*/
      262144 && t_value !== (t_value = /*obj*/
      (ctx2[644].display || /*obj*/
      ctx2[644].label || `#${/*obj*/
      ctx2[644].id}`) + "")) set_data(t, t_value);
      if (dirty[1] & /*activeAllObjs*/
      262144 && option_value_value !== (option_value_value = /*obj*/
      ctx2[644].id)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_68(key_1, ctx) {
  let tr;
  let t0;
  let td0;
  let t1_value = (
    /*vr*/
    ctx[641].id + ""
  );
  let t1;
  let t2;
  let t3;
  let td1;
  let t4;
  let td2;
  let t5;
  let td3;
  let t6;
  let td4;
  let t7;
  let each_blocks = [];
  let each_1_lookup = /* @__PURE__ */ new Map();
  let t8;
  let if_block3_anchor;
  function select_block_type_7(ctx2, dirty) {
    if (
      /*vrMode*/
      ctx2[638] === "edit"
    ) return create_if_block_80;
    return create_else_block_10;
  }
  let current_block_type = select_block_type_7(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_8(ctx2, dirty) {
    if (
      /*vrMode*/
      ctx2[638] === "edit"
    ) return create_if_block_79;
    return create_else_block_9;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_79) return get_if_ctx_4(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_8(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  let if_block2 = !/*vrMode*/
  ctx[638] && !/*vrAddAntState*/
  ctx[639] && create_if_block_78(ctx);
  let each_value_70 = ensure_array_like(
    /*vr*/
    ctx[641].refs || []
  );
  const get_key = (ctx2) => (
    /*ref*/
    ctx2[647].id
  );
  for (let i = 0; i < each_value_70.length; i += 1) {
    let child_ctx = get_each_context_70(ctx, each_value_70, i);
    let key = get_key(child_ctx);
    each_1_lookup.set(key, each_blocks[i] = create_each_block_70(key, child_ctx));
  }
  let if_block3 = (
    /*vrAddAntState*/
    ctx[639] && /*vrAddAntState*/
    ctx[639].vrId === /*vr*/
    ctx[641].id && create_if_block_75(ctx)
  );
  return {
    key: key_1,
    first: null,
    c() {
      tr = element("tr");
      if_block0.c();
      t0 = space();
      td0 = element("td");
      t1 = text(t1_value);
      t2 = space();
      if_block1.c();
      t3 = space();
      td1 = element("td");
      t4 = space();
      td2 = element("td");
      t5 = space();
      td3 = element("td");
      t6 = space();
      td4 = element("td");
      if (if_block2) if_block2.c();
      t7 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t8 = space();
      if (if_block3) if_block3.c();
      if_block3_anchor = empty();
      attr(tr, "class", "vr-main-row");
      this.first = tr;
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      if_block0.m(tr, null);
      append(tr, t0);
      append(tr, td0);
      append(td0, t1);
      append(tr, t2);
      if_block1.m(tr, null);
      append(tr, t3);
      append(tr, td1);
      append(tr, t4);
      append(tr, td2);
      append(tr, t5);
      append(tr, td3);
      append(tr, t6);
      append(tr, td4);
      if (if_block2) if_block2.m(td4, null);
      insert(target, t7, anchor);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, t8, anchor);
      if (if_block3) if_block3.m(target, anchor);
      insert(target, if_block3_anchor, anchor);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_7(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr, t0);
        }
      }
      if (dirty[1] & /*activeVrs*/
      1048576 && t1_value !== (t1_value = /*vr*/
      ctx[641].id + "")) set_data(t1, t1_value);
      if (current_block_type_1 === (current_block_type_1 = select_block_type_8(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr, t3);
        }
      }
      if (!/*vrMode*/
      ctx[638] && !/*vrAddAntState*/
      ctx[639]) {
        if (if_block2) {
          if_block2.p(ctx, dirty);
        } else {
          if_block2 = create_if_block_78(ctx);
          if_block2.c();
          if_block2.m(td4, null);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (dirty[0] & /*smVrEditForms, activeSerial, smVrMode, smVrSelected*/
      27262977 | dirty[1] & /*activeVrs, activeAllObjs*/
      1310720 | dirty[4] & /*smObjDisplay, smObjRxOnly, smVrToggleSelect*/
      1036) {
        each_value_70 = ensure_array_like(
          /*vr*/
          ctx[641].refs || []
        );
        each_blocks = update_keyed_each(each_blocks, dirty, get_key, 1, ctx, each_value_70, each_1_lookup, t8.parentNode, destroy_block, create_each_block_70, t8, get_each_context_70);
      }
      if (
        /*vrAddAntState*/
        ctx[639] && /*vrAddAntState*/
        ctx[639].vrId === /*vr*/
        ctx[641].id
      ) {
        if (if_block3) {
          if_block3.p(ctx, dirty);
        } else {
          if_block3 = create_if_block_75(ctx);
          if_block3.c();
          if_block3.m(if_block3_anchor.parentNode, if_block3_anchor);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
        detach(t7);
        detach(t8);
        detach(if_block3_anchor);
      }
      if_block0.d();
      if_block1.d();
      if (if_block2) if_block2.d();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].d(detaching);
      }
      if (if_block3) if_block3.d(detaching);
    }
  };
}
function create_if_block_74(ctx) {
  let tr;
  let td0;
  let t0;
  let td1;
  let t1;
  let td2;
  let input0;
  let t2;
  let td3;
  let input1;
  let t3;
  let td4;
  let t4;
  let td5;
  let t5;
  let td6;
  let t6;
  let td7;
  let mounted;
  let dispose;
  return {
    c() {
      tr = element("tr");
      td0 = element("td");
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      input0 = element("input");
      t2 = space();
      td3 = element("td");
      input1 = element("input");
      t3 = space();
      td4 = element("td");
      t4 = space();
      td5 = element("td");
      t5 = space();
      td6 = element("td");
      t6 = space();
      td7 = element("td");
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "5");
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      attr(input1, "size", "10");
      attr(tr, "class", "add-row");
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      append(tr, td0);
      append(tr, t0);
      append(tr, td1);
      append(tr, t1);
      append(tr, td2);
      append(td2, input0);
      set_input_value(
        input0,
        /*smVrAddForm*/
        ctx[22][
          /*activeSerial*/
          ctx[0]
        ].label
      );
      append(tr, t2);
      append(tr, td3);
      append(td3, input1);
      set_input_value(
        input1,
        /*smVrAddForm*/
        ctx[22][
          /*activeSerial*/
          ctx[0]
        ].display
      );
      append(tr, t3);
      append(tr, td4);
      append(tr, t4);
      append(tr, td5);
      append(tr, t5);
      append(tr, td6);
      append(tr, t6);
      append(tr, td7);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler_2*/
            ctx[376]
          ),
          listen(
            input1,
            "input",
            /*input1_input_handler_2*/
            ctx[377]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smVrAddForm, activeSerial*/
      4194305 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0.value !== /*smVrAddForm*/
      ctx2[22][
        /*activeSerial*/
        ctx2[0]
      ].label) {
        set_input_value(
          input0,
          /*smVrAddForm*/
          ctx2[22][
            /*activeSerial*/
            ctx2[0]
          ].label
        );
      }
      if (dirty[0] & /*smVrAddForm, activeSerial*/
      4194305 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1.value !== /*smVrAddForm*/
      ctx2[22][
        /*activeSerial*/
        ctx2[0]
      ].display) {
        set_input_value(
          input1,
          /*smVrAddForm*/
          ctx2[22][
            /*activeSerial*/
            ctx2[0]
          ].display
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_else_block_6(ctx) {
  let button0;
  let t1;
  let button1;
  let t3;
  let button2;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Add";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Edit";
      t3 = space();
      button2 = element("button");
      button2.textContent = "Remove";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
      attr(button2, "class", "btn");
      attr(button2, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      insert(target, t3, anchor);
      insert(target, button2, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_26*/
            ctx[384]
          ),
          listen(
            button1,
            "click",
            /*click_handler_27*/
            ctx[385]
          ),
          listen(
            button2,
            "click",
            /*click_handler_28*/
            ctx[386]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
        detach(t3);
        detach(button2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_73(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_24*/
            ctx[382]
          ),
          listen(
            button1,
            "click",
            /*click_handler_25*/
            ctx[383]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_72(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_22*/
            ctx[380]
          ),
          listen(
            button1,
            "click",
            /*click_handler_23*/
            ctx[381]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_71(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_20*/
            ctx[378]
          ),
          listen(
            button1,
            "click",
            /*click_handler_21*/
            ctx[379]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_70(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block_67(ctx) {
  let th;
  let t_value = (
    /*oc*/
    ctx[624] + ""
  );
  let t;
  return {
    c() {
      th = element("th");
      t = text(t_value);
    },
    m(target, anchor) {
      insert(target, th, anchor);
      append(th, t);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAntOutCols*/
      2097152 && t_value !== (t_value = /*oc*/
      ctx2[624] + "")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(th);
      }
    }
  };
}
function create_else_block_5(ctx) {
  let td;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  function change_handler_120() {
    return (
      /*change_handler_120*/
      ctx[341](
        /*ant*/
        ctx[627]
      )
    );
  }
  return {
    c() {
      td = element("td");
      input = element("input");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = /*smAntSelected*/
      (ctx[20][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*ant*/
        ctx[627].id
      );
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, input);
      if (!mounted) {
        dispose = listen(input, "change", change_handler_120);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smAntSelected, activeSerial*/
      1048577 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && input_checked_value !== (input_checked_value = /*smAntSelected*/
      (ctx[20][
        /*activeSerial*/
        ctx[0]
      ] || []).includes(
        /*ant*/
        ctx[627].id
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_68(ctx) {
  let td;
  return {
    c() {
      td = element("td");
    },
    m(target, anchor) {
      insert(target, td, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_else_block_4(ctx) {
  let td0;
  let t0_value = (
    /*ant*/
    (ctx[627].label || "") + ""
  );
  let t0;
  let t1;
  let td1;
  let t2_value = (
    /*ant*/
    (ctx[627].display || "") + ""
  );
  let t2;
  let t3;
  let td2;
  let t4_value = (
    /*ant*/
    ctx[627].steppir ? "Yes" : "No"
  );
  let t4;
  let t5;
  let td3;
  let t6_value = (
    /*ant*/
    ctx[627].rxonly ? "Yes" : "No"
  );
  let t6;
  let t7;
  let td4;
  let t8_value = (
    /*ant*/
    (ctx[627].pa_ant_number || 0) + ""
  );
  let t8;
  let t9;
  let td5;
  let t10_value = (
    /*ant*/
    ctx[627].rotator ? "Yes" : "No"
  );
  let t10;
  let t11;
  let td6;
  let t12_value = (
    /*ant*/
    (ctx[627].rotator_offset || 0) + ""
  );
  let t12;
  let t13;
  let each_1_anchor;
  let each_value_66 = ensure_array_like(
    /*antOutCols*/
    ctx[622]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_66.length; i += 1) {
    each_blocks[i] = create_each_block_66(get_each_context_66(ctx, each_value_66, i));
  }
  return {
    c() {
      td0 = element("td");
      t0 = text(t0_value);
      t1 = space();
      td1 = element("td");
      t2 = text(t2_value);
      t3 = space();
      td2 = element("td");
      t4 = text(t4_value);
      t5 = space();
      td3 = element("td");
      t6 = text(t6_value);
      t7 = space();
      td4 = element("td");
      t8 = text(t8_value);
      t9 = space();
      td5 = element("td");
      t10 = text(t10_value);
      t11 = space();
      td6 = element("td");
      t12 = text(t12_value);
      t13 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, t0);
      insert(target, t1, anchor);
      insert(target, td1, anchor);
      append(td1, t2);
      insert(target, t3, anchor);
      insert(target, td2, anchor);
      append(td2, t4);
      insert(target, t5, anchor);
      insert(target, td3, anchor);
      append(td3, t6);
      insert(target, t7, anchor);
      insert(target, td4, anchor);
      append(td4, t8);
      insert(target, t9, anchor);
      insert(target, td5, anchor);
      append(td5, t10);
      insert(target, t11, anchor);
      insert(target, td6, anchor);
      append(td6, t12);
      insert(target, t13, anchor);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty[1] & /*activeAnts*/
      4194304 && t0_value !== (t0_value = /*ant*/
      (ctx2[627].label || "") + "")) set_data(t0, t0_value);
      if (dirty[1] & /*activeAnts*/
      4194304 && t2_value !== (t2_value = /*ant*/
      (ctx2[627].display || "") + "")) set_data(t2, t2_value);
      if (dirty[1] & /*activeAnts*/
      4194304 && t4_value !== (t4_value = /*ant*/
      ctx2[627].steppir ? "Yes" : "No")) set_data(t4, t4_value);
      if (dirty[1] & /*activeAnts*/
      4194304 && t6_value !== (t6_value = /*ant*/
      ctx2[627].rxonly ? "Yes" : "No")) set_data(t6, t6_value);
      if (dirty[1] & /*activeAnts*/
      4194304 && t8_value !== (t8_value = /*ant*/
      (ctx2[627].pa_ant_number || 0) + "")) set_data(t8, t8_value);
      if (dirty[1] & /*activeAnts*/
      4194304 && t10_value !== (t10_value = /*ant*/
      ctx2[627].rotator ? "Yes" : "No")) set_data(t10, t10_value);
      if (dirty[1] & /*activeAnts*/
      4194304 && t12_value !== (t12_value = /*ant*/
      (ctx2[627].rotator_offset || 0) + "")) set_data(t12, t12_value);
      if (dirty[1] & /*activeAnts, activeAntOutCols*/
      6291456) {
        each_value_66 = ensure_array_like(
          /*antOutCols*/
          ctx2[622]
        );
        let i;
        for (i = 0; i < each_value_66.length; i += 1) {
          const child_ctx = get_each_context_66(ctx2, each_value_66, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_66(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_66.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t1);
        detach(td1);
        detach(t3);
        detach(td2);
        detach(t5);
        detach(td3);
        detach(t7);
        detach(td4);
        detach(t9);
        detach(td5);
        detach(t11);
        detach(td6);
        detach(t13);
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_67(ctx) {
  let td0;
  let input0;
  let input0_value_value;
  let t0;
  let td1;
  let input1;
  let input1_value_value;
  let t1;
  let td2;
  let select0;
  let option0;
  let option1;
  let select0_value_value;
  let t4;
  let td3;
  let select1;
  let option2;
  let option3;
  let select1_value_value;
  let t7;
  let td4;
  let input2;
  let input2_value_value;
  let t8;
  let td5;
  let select2;
  let option4;
  let option5;
  let select2_value_value;
  let t11;
  let td6;
  let input3;
  let input3_value_value;
  let t12;
  let each_1_anchor;
  let mounted;
  let dispose;
  function input_handler_37(...args) {
    return (
      /*input_handler_37*/
      ctx[342](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  function input_handler_38(...args) {
    return (
      /*input_handler_38*/
      ctx[343](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  function change_handler_121(...args) {
    return (
      /*change_handler_121*/
      ctx[344](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  function change_handler_122(...args) {
    return (
      /*change_handler_122*/
      ctx[345](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  function input_handler_39(...args) {
    return (
      /*input_handler_39*/
      ctx[346](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  function change_handler_123(...args) {
    return (
      /*change_handler_123*/
      ctx[347](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  function input_handler_40(...args) {
    return (
      /*input_handler_40*/
      ctx[348](
        /*ant*/
        ctx[627],
        ...args
      )
    );
  }
  let each_value_65 = ensure_array_like(
    /*antOutCols*/
    ctx[622]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_65.length; i += 1) {
    each_blocks[i] = create_each_block_65(get_each_context_65(ctx, each_value_65, i));
  }
  return {
    c() {
      td0 = element("td");
      input0 = element("input");
      t0 = space();
      td1 = element("td");
      input1 = element("input");
      t1 = space();
      td2 = element("td");
      select0 = element("select");
      option0 = element("option");
      option0.textContent = "No";
      option1 = element("option");
      option1.textContent = "Yes";
      t4 = space();
      td3 = element("td");
      select1 = element("select");
      option2 = element("option");
      option2.textContent = "No";
      option3 = element("option");
      option3.textContent = "Yes";
      t7 = space();
      td4 = element("td");
      input2 = element("input");
      t8 = space();
      td5 = element("td");
      select2 = element("select");
      option4 = element("option");
      option4.textContent = "No";
      option5 = element("option");
      option5.textContent = "Yes";
      t11 = space();
      td6 = element("td");
      input3 = element("input");
      t12 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "5");
      input0.value = input0_value_value = /*ef*/
      ctx[630].label;
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      attr(input1, "size", "10");
      input1.value = input1_value_value = /*ef*/
      ctx[630].display;
      option0.__value = 0;
      set_input_value(option0, option0.__value);
      option1.__value = 1;
      set_input_value(option1, option1.__value);
      attr(select0, "class", "select");
      option2.__value = 0;
      set_input_value(option2, option2.__value);
      option3.__value = 1;
      set_input_value(option3, option3.__value);
      attr(select1, "class", "select");
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      set_style(input2, "width", "60px");
      input2.value = input2_value_value = /*ef*/
      ctx[630].pa_ant_number;
      option4.__value = 0;
      set_input_value(option4, option4.__value);
      option5.__value = 1;
      set_input_value(option5, option5.__value);
      attr(select2, "class", "select");
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      set_style(input3, "width", "70px");
      input3.value = input3_value_value = /*ef*/
      ctx[630].rotator_offset;
    },
    m(target, anchor) {
      insert(target, td0, anchor);
      append(td0, input0);
      insert(target, t0, anchor);
      insert(target, td1, anchor);
      append(td1, input1);
      insert(target, t1, anchor);
      insert(target, td2, anchor);
      append(td2, select0);
      append(select0, option0);
      append(select0, option1);
      select_option(
        select0,
        /*ef*/
        ctx[630].steppir
      );
      insert(target, t4, anchor);
      insert(target, td3, anchor);
      append(td3, select1);
      append(select1, option2);
      append(select1, option3);
      select_option(
        select1,
        /*ef*/
        ctx[630].rxonly
      );
      insert(target, t7, anchor);
      insert(target, td4, anchor);
      append(td4, input2);
      insert(target, t8, anchor);
      insert(target, td5, anchor);
      append(td5, select2);
      append(select2, option4);
      append(select2, option5);
      select_option(
        select2,
        /*ef*/
        ctx[630].rotator
      );
      insert(target, t11, anchor);
      insert(target, td6, anchor);
      append(td6, input3);
      insert(target, t12, anchor);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
      if (!mounted) {
        dispose = [
          listen(input0, "input", input_handler_37),
          listen(input1, "input", input_handler_38),
          listen(select0, "change", change_handler_121),
          listen(select1, "change", change_handler_122),
          listen(input2, "input", input_handler_39),
          listen(select2, "change", change_handler_123),
          listen(input3, "input", input_handler_40)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && input0_value_value !== (input0_value_value = /*ef*/
      ctx[630].label) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && input1_value_value !== (input1_value_value = /*ef*/
      ctx[630].display) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && select0_value_value !== (select0_value_value = /*ef*/
      ctx[630].steppir)) {
        select_option(
          select0,
          /*ef*/
          ctx[630].steppir
        );
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && select1_value_value !== (select1_value_value = /*ef*/
      ctx[630].rxonly)) {
        select_option(
          select1,
          /*ef*/
          ctx[630].rxonly
        );
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && input2_value_value !== (input2_value_value = /*ef*/
      ctx[630].pa_ant_number) && input2.value !== input2_value_value) {
        input2.value = input2_value_value;
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && select2_value_value !== (select2_value_value = /*ef*/
      ctx[630].rotator)) {
        select_option(
          select2,
          /*ef*/
          ctx[630].rotator
        );
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntsAndGroups*/
      4202496 && input3_value_value !== (input3_value_value = /*ef*/
      ctx[630].rotator_offset) && input3.value !== input3_value_value) {
        input3.value = input3_value_value;
      }
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntOutCols*/
      6291456) {
        each_value_65 = ensure_array_like(
          /*antOutCols*/
          ctx[622]
        );
        let i;
        for (i = 0; i < each_value_65.length; i += 1) {
          const child_ctx = get_each_context_65(ctx, each_value_65, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_65(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_65.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td0);
        detach(t0);
        detach(td1);
        detach(t1);
        detach(td2);
        detach(t4);
        detach(td3);
        detach(t7);
        detach(td4);
        detach(t8);
        detach(td5);
        detach(t11);
        detach(td6);
        detach(t12);
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_66(ctx) {
  var _a;
  let td;
  let t_value = (
    /*ant*/
    ((_a = ctx[627].output) == null ? void 0 : _a[
      /*oc*/
      ctx[624]
    ]) ? "Yes" : "No"
  );
  let t;
  return {
    c() {
      td = element("td");
      t = text(t_value);
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, t);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty[1] & /*activeAnts, activeAntOutCols*/
      6291456 && t_value !== (t_value = /*ant*/
      ((_a2 = ctx2[627].output) == null ? void 0 : _a2[
        /*oc*/
        ctx2[624]
      ]) ? "Yes" : "No")) set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
    }
  };
}
function create_each_block_65(ctx) {
  let td;
  let select;
  let option0;
  let option1;
  let select_value_value;
  let mounted;
  let dispose;
  function change_handler_124(...args) {
    return (
      /*change_handler_124*/
      ctx[349](
        /*ant*/
        ctx[627],
        /*oc*/
        ctx[624],
        ...args
      )
    );
  }
  return {
    c() {
      td = element("td");
      select = element("select");
      option0 = element("option");
      option0.textContent = "No";
      option1 = element("option");
      option1.textContent = "Yes";
      option0.__value = 0;
      set_input_value(option0, option0.__value);
      option1.__value = 1;
      set_input_value(option1, option1.__value);
      attr(select, "class", "select");
    },
    m(target, anchor) {
      var _a;
      insert(target, td, anchor);
      append(td, select);
      append(select, option0);
      append(select, option1);
      select_option(
        select,
        /*ef*/
        ((_a = ctx[630].output) == null ? void 0 : _a[
          /*oc*/
          ctx[624]
        ]) ?? 0
      );
      if (!mounted) {
        dispose = listen(select, "change", change_handler_124);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a, _b;
      ctx = new_ctx;
      if (dirty[0] & /*smAntEditForms, activeSerial*/
      262145 | dirty[1] & /*activeAnts, activeAntOutCols, activeAntsAndGroups*/
      6299648 && select_value_value !== (select_value_value = /*ef*/
      ((_a = ctx[630].output) == null ? void 0 : _a[
        /*oc*/
        ctx[624]
      ]) ?? 0)) {
        select_option(
          select,
          /*ef*/
          ((_b = ctx[630].output) == null ? void 0 : _b[
            /*oc*/
            ctx[624]
          ]) ?? 0
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_64(key_1, ctx) {
  let tr;
  let t0;
  let td;
  let t1_value = (
    /*ant*/
    ctx[627].id + ""
  );
  let t1;
  let t2;
  function select_block_type_4(ctx2, dirty) {
    if (
      /*mode*/
      ctx2[547] === "edit"
    ) return create_if_block_68;
    return create_else_block_5;
  }
  let current_block_type = select_block_type_4(ctx);
  let if_block0 = current_block_type(ctx);
  function select_block_type_5(ctx2, dirty) {
    if (
      /*mode*/
      ctx2[547] === "edit"
    ) return create_if_block_67;
    return create_else_block_4;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_67) return get_if_ctx_1(ctx2);
    return ctx2;
  }
  let current_block_type_1 = select_block_type_5(ctx);
  let if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
  return {
    key: key_1,
    first: null,
    c() {
      tr = element("tr");
      if_block0.c();
      t0 = space();
      td = element("td");
      t1 = text(t1_value);
      t2 = space();
      if_block1.c();
      this.first = tr;
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      if_block0.m(tr, null);
      append(tr, t0);
      append(tr, td);
      append(td, t1);
      append(tr, t2);
      if_block1.m(tr, null);
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (current_block_type === (current_block_type = select_block_type_4(ctx)) && if_block0) {
        if_block0.p(ctx, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx);
        if (if_block0) {
          if_block0.c();
          if_block0.m(tr, t0);
        }
      }
      if (dirty[1] & /*activeAnts*/
      4194304 && t1_value !== (t1_value = /*ant*/
      ctx[627].id + "")) set_data(t1, t1_value);
      if (current_block_type_1 === (current_block_type_1 = select_block_type_5(ctx)) && if_block1) {
        if_block1.p(select_block_ctx(ctx, current_block_type_1), dirty);
      } else {
        if_block1.d(1);
        if_block1 = current_block_type_1(select_block_ctx(ctx, current_block_type_1));
        if (if_block1) {
          if_block1.c();
          if_block1.m(tr, null);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      if_block0.d();
      if_block1.d();
    }
  };
}
function create_if_block_66(ctx) {
  let tr;
  let td0;
  let t0;
  let td1;
  let t1;
  let td2;
  let input0;
  let t2;
  let td3;
  let input1;
  let t3;
  let td4;
  let select0;
  let option0;
  let option1;
  let t6;
  let td5;
  let select1;
  let option2;
  let option3;
  let t9;
  let td6;
  let input2;
  let t10;
  let td7;
  let select2;
  let option4;
  let option5;
  let t13;
  let td8;
  let input3;
  let t14;
  let mounted;
  let dispose;
  let each_value_63 = ensure_array_like(
    /*antOutCols*/
    ctx[622]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_63.length; i += 1) {
    each_blocks[i] = create_each_block_63(get_each_context_63(ctx, each_value_63, i));
  }
  return {
    c() {
      tr = element("tr");
      td0 = element("td");
      t0 = space();
      td1 = element("td");
      t1 = space();
      td2 = element("td");
      input0 = element("input");
      t2 = space();
      td3 = element("td");
      input1 = element("input");
      t3 = space();
      td4 = element("td");
      select0 = element("select");
      option0 = element("option");
      option0.textContent = "No";
      option1 = element("option");
      option1.textContent = "Yes";
      t6 = space();
      td5 = element("td");
      select1 = element("select");
      option2 = element("option");
      option2.textContent = "No";
      option3 = element("option");
      option3.textContent = "Yes";
      t9 = space();
      td6 = element("td");
      input2 = element("input");
      t10 = space();
      td7 = element("td");
      select2 = element("select");
      option4 = element("option");
      option4.textContent = "No";
      option5 = element("option");
      option5.textContent = "Yes";
      t13 = space();
      td8 = element("td");
      input3 = element("input");
      t14 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(input0, "class", "input");
      attr(input0, "type", "text");
      attr(input0, "size", "5");
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      attr(input1, "size", "10");
      option0.__value = 0;
      set_input_value(option0, option0.__value);
      option1.__value = 1;
      set_input_value(option1, option1.__value);
      attr(select0, "class", "select");
      if (
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].steppir === void 0
      ) add_render_callback(() => (
        /*select0_change_handler*/
        ctx[352].call(select0)
      ));
      option2.__value = 0;
      set_input_value(option2, option2.__value);
      option3.__value = 1;
      set_input_value(option3, option3.__value);
      attr(select1, "class", "select");
      if (
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].rxonly === void 0
      ) add_render_callback(() => (
        /*select1_change_handler*/
        ctx[353].call(select1)
      ));
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      set_style(input2, "width", "60px");
      option4.__value = 0;
      set_input_value(option4, option4.__value);
      option5.__value = 1;
      set_input_value(option5, option5.__value);
      attr(select2, "class", "select");
      if (
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].rotator === void 0
      ) add_render_callback(() => (
        /*select2_change_handler*/
        ctx[355].call(select2)
      ));
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      set_style(input3, "width", "70px");
      attr(tr, "class", "add-row");
    },
    m(target, anchor) {
      insert(target, tr, anchor);
      append(tr, td0);
      append(tr, t0);
      append(tr, td1);
      append(tr, t1);
      append(tr, td2);
      append(td2, input0);
      set_input_value(
        input0,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].label
      );
      append(tr, t2);
      append(tr, td3);
      append(td3, input1);
      set_input_value(
        input1,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].display
      );
      append(tr, t3);
      append(tr, td4);
      append(td4, select0);
      append(select0, option0);
      append(select0, option1);
      select_option(
        select0,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].steppir,
        true
      );
      append(tr, t6);
      append(tr, td5);
      append(td5, select1);
      append(select1, option2);
      append(select1, option3);
      select_option(
        select1,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].rxonly,
        true
      );
      append(tr, t9);
      append(tr, td6);
      append(td6, input2);
      set_input_value(
        input2,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].pa_ant_number
      );
      append(tr, t10);
      append(tr, td7);
      append(td7, select2);
      append(select2, option4);
      append(select2, option5);
      select_option(
        select2,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].rotator,
        true
      );
      append(tr, t13);
      append(tr, td8);
      append(td8, input3);
      set_input_value(
        input3,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].rotator_offset
      );
      append(tr, t14);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(tr, null);
        }
      }
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input0_input_handler*/
            ctx[350]
          ),
          listen(
            input1,
            "input",
            /*input1_input_handler*/
            ctx[351]
          ),
          listen(
            select0,
            "change",
            /*select0_change_handler*/
            ctx[352]
          ),
          listen(
            select1,
            "change",
            /*select1_change_handler*/
            ctx[353]
          ),
          listen(
            input2,
            "input",
            /*input2_input_handler*/
            ctx[354]
          ),
          listen(
            select2,
            "change",
            /*select2_change_handler*/
            ctx[355]
          ),
          listen(
            input3,
            "input",
            /*input3_input_handler*/
            ctx[356]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0.value !== /*smAntAddForm*/
      ctx2[19][
        /*activeSerial*/
        ctx2[0]
      ].label) {
        set_input_value(
          input0,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].label
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1.value !== /*smAntAddForm*/
      ctx2[19][
        /*activeSerial*/
        ctx2[0]
      ].display) {
        set_input_value(
          input1,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].display
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192) {
        select_option(
          select0,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].steppir
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192) {
        select_option(
          select1,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].rxonly
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192 && to_number(input2.value) !== /*smAntAddForm*/
      ctx2[19][
        /*activeSerial*/
        ctx2[0]
      ].pa_ant_number) {
        set_input_value(
          input2,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].pa_ant_number
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192) {
        select_option(
          select2,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].rotator
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntsAndGroups*/
      8192 && to_number(input3.value) !== /*smAntAddForm*/
      ctx2[19][
        /*activeSerial*/
        ctx2[0]
      ].rotator_offset) {
        set_input_value(
          input3,
          /*smAntAddForm*/
          ctx2[19][
            /*activeSerial*/
            ctx2[0]
          ].rotator_offset
        );
      }
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntOutCols*/
      2097152) {
        each_value_63 = ensure_array_like(
          /*antOutCols*/
          ctx2[622]
        );
        let i;
        for (i = 0; i < each_value_63.length; i += 1) {
          const child_ctx = get_each_context_63(ctx2, each_value_63, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_63(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(tr, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_63.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(tr);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_63(ctx) {
  let td;
  let select;
  let option0;
  let option1;
  let mounted;
  let dispose;
  function select_change_handler() {
    ctx[357].call(
      select,
      /*oc*/
      ctx[624]
    );
  }
  return {
    c() {
      td = element("td");
      select = element("select");
      option0 = element("option");
      option0.textContent = "No";
      option1 = element("option");
      option1.textContent = "Yes";
      option0.__value = 0;
      set_input_value(option0, option0.__value);
      option1.__value = 1;
      set_input_value(option1, option1.__value);
      attr(select, "class", "select");
      if (
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].output[
          /*oc*/
          ctx[624]
        ] === void 0
      ) add_render_callback(select_change_handler);
    },
    m(target, anchor) {
      insert(target, td, anchor);
      append(td, select);
      append(select, option0);
      append(select, option1);
      select_option(
        select,
        /*smAntAddForm*/
        ctx[19][
          /*activeSerial*/
          ctx[0]
        ].output[
          /*oc*/
          ctx[624]
        ],
        true
      );
      if (!mounted) {
        dispose = listen(select, "change", select_change_handler);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*smAntAddForm, activeSerial*/
      524289 | dirty[1] & /*activeAntOutCols, activeAntsAndGroups*/
      2105344) {
        select_option(
          select,
          /*smAntAddForm*/
          ctx[19][
            /*activeSerial*/
            ctx[0]
          ].output[
            /*oc*/
            ctx[624]
          ]
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(td);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_else_block_3(ctx) {
  let button0;
  let t1;
  let button1;
  let t3;
  let button2;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Add";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Edit";
      t3 = space();
      button2 = element("button");
      button2.textContent = "Remove";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
      attr(button2, "class", "btn");
      attr(button2, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      insert(target, t3, anchor);
      insert(target, button2, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_16*/
            ctx[362]
          ),
          listen(
            button1,
            "click",
            /*click_handler_17*/
            ctx[363]
          ),
          listen(
            button2,
            "click",
            /*click_handler_18*/
            ctx[364]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
        detach(t3);
        detach(button2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_65(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_14*/
            ctx[360]
          ),
          listen(
            button1,
            "click",
            /*click_handler_15*/
            ctx[361]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_64(ctx) {
  let button0;
  let t1;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      button0 = element("button");
      button0.textContent = "Save";
      t1 = space();
      button1 = element("button");
      button1.textContent = "Cancel";
      attr(button0, "class", "btn");
      attr(button0, "type", "button");
      attr(button1, "class", "btn");
      attr(button1, "type", "button");
    },
    m(target, anchor) {
      insert(target, button0, anchor);
      insert(target, t1, anchor);
      insert(target, button1, anchor);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_12*/
            ctx[358]
          ),
          listen(
            button1,
            "click",
            /*click_handler_13*/
            ctx[359]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button0);
        detach(t1);
        detach(button1);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_63(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block_62(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_61(ctx) {
  let div2;
  let div0;
  let t2;
  let div1;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  let each_value_62 = ensure_array_like(
    /*smOutputClassOptions*/
    ctx[84]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_62.length; i += 1) {
    each_blocks[i] = create_each_block_62(get_each_context_62(ctx, each_value_62, i));
  }
  function change_handler_119(...args) {
    return (
      /*change_handler_119*/
      ctx[340](
        /*output*/
        ctx[606],
        ...args
      )
    );
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = `${/*output*/
      ctx[606]}:`;
      t2 = space();
      div1 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "label");
      attr(select, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t2);
      append(div2, div1);
      append(div1, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*smOutputVal*/
        ctx[113](
          /*activeSerial*/
          ctx[0],
          /*output*/
          ctx[606]
        )
      );
      if (!mounted) {
        dispose = listen(select, "change", change_handler_119);
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[2] & /*smOutputClassOptions*/
      4194304) {
        each_value_62 = ensure_array_like(
          /*smOutputClassOptions*/
          ctx[84]
        );
        let i;
        for (i = 0; i < each_value_62.length; i += 1) {
          const child_ctx = get_each_context_62(ctx, each_value_62, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_62(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_62.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select_value_value !== (select_value_value = /*smOutputVal*/
      ctx[113](
        /*activeSerial*/
        ctx[0],
        /*output*/
        ctx[606]
      ))) {
        select_option(
          select,
          /*smOutputVal*/
          ctx[113](
            /*activeSerial*/
            ctx[0],
            /*output*/
            ctx[606]
          )
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_61(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block_60(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_59(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_58(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_57(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_59(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_58(ctx) {
  let section;
  let div0;
  let t1;
  let div4;
  let div3;
  let t7;
  let each_value_56 = ensure_array_like(
    /*smSequencerOutputs*/
    ctx[114](
      /*activeSerial*/
      ctx[0]
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_56.length; i += 1) {
    each_blocks[i] = create_each_block_56(get_each_context_56(ctx, each_value_56, i));
  }
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Sequencer";
      t1 = space();
      div4 = element("div");
      div3 = element("div");
      div3.innerHTML = `<div class="label"> </div> <div class="value" style="display: flex; gap: 2em;"><span style="width: 80px;">Lead</span> <span style="width: 80px;">Tail</span></div>`;
      t7 = space();
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "section-title");
      attr(div3, "class", "row");
      set_style(div3, "font-weight", "700");
      attr(div4, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div4);
      append(div4, div3);
      append(div4, t7);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div4, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[3] & /*smSequencerVal, smSequencerOutputs, applySmSequencer*/
      11010048) {
        each_value_56 = ensure_array_like(
          /*smSequencerOutputs*/
          ctx2[114](
            /*activeSerial*/
            ctx2[0]
          )
        );
        let i;
        for (i = 0; i < each_value_56.length; i += 1) {
          const child_ctx = get_each_context_56(ctx2, each_value_56, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_56(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div4, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_56.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_each_block_56(ctx) {
  let div2;
  let div0;
  let t0_value = (
    /*output*/
    ctx[606] + ""
  );
  let t0;
  let t1;
  let t2;
  let div1;
  let input0;
  let input0_value_value;
  let t3;
  let input1;
  let input1_value_value;
  let t4;
  let mounted;
  let dispose;
  function change_handler_117(...args) {
    return (
      /*change_handler_117*/
      ctx[338](
        /*output*/
        ctx[606],
        ...args
      )
    );
  }
  function change_handler_118(...args) {
    return (
      /*change_handler_118*/
      ctx[339](
        /*output*/
        ctx[606],
        ...args
      )
    );
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      t0 = text(t0_value);
      t1 = text(":");
      t2 = space();
      div1 = element("div");
      input0 = element("input");
      t3 = space();
      input1 = element("input");
      t4 = space();
      attr(div0, "class", "label");
      attr(input0, "class", "input");
      set_style(input0, "width", "80px");
      attr(input0, "type", "number");
      attr(input0, "min", "0");
      input0.value = input0_value_value = /*smSequencerVal*/
      ctx[112](
        /*activeSerial*/
        ctx[0],
        "lead",
        /*output*/
        ctx[606]
      );
      attr(input1, "class", "input");
      set_style(input1, "width", "80px");
      attr(input1, "type", "number");
      attr(input1, "min", "0");
      input1.value = input1_value_value = /*smSequencerVal*/
      ctx[112](
        /*activeSerial*/
        ctx[0],
        "tail",
        /*output*/
        ctx[606]
      );
      attr(div1, "class", "value");
      set_style(div1, "display", "flex");
      set_style(div1, "gap", "2em");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div0, t0);
      append(div0, t1);
      append(div2, t2);
      append(div2, div1);
      append(div1, input0);
      append(div1, t3);
      append(div1, input1);
      append(div2, t4);
      if (!mounted) {
        dispose = [
          listen(input0, "change", change_handler_117),
          listen(input1, "change", change_handler_118)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*activeSerial*/
      1 && t0_value !== (t0_value = /*output*/
      ctx[606] + "")) set_data(t0, t0_value);
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*smSequencerVal*/
      ctx[112](
        /*activeSerial*/
        ctx[0],
        "lead",
        /*output*/
        ctx[606]
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_value_value !== (input1_value_value = /*smSequencerVal*/
      ctx[112](
        /*activeSerial*/
        ctx[0],
        "tail",
        /*output*/
        ctx[606]
      )) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_56(ctx) {
  let div;
  let t_value = (
    /*smActionStatus*/
    ctx[15][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*smActionStatus*/
      ctx[15][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*smActionStatus, activeSerial*/
      32769 && t_value !== (t_value = /*smActionStatus*/
      ctx2[15][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*smActionStatus, activeSerial*/
      32769 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*smActionStatus*/
      ctx2[15][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block_55(ctx) {
  let div2;
  let div0;
  let t3;
  let div1;
  let input;
  let input_value_value;
  let t4;
  let button;
  let mounted;
  let dispose;
  function input_handler_35(...args) {
    return (
      /*input_handler_35*/
      ctx[320](
        /*idx*/
        ctx[601],
        ...args
      )
    );
  }
  function click_handler_6() {
    return (
      /*click_handler_6*/
      ctx[321](
        /*idx*/
        ctx[601]
      )
    );
  }
  return {
    c() {
      var _a, _b;
      div2 = element("div");
      div0 = element("div");
      div0.textContent = `Message #${/*idx*/
      ctx[601]}:`;
      t3 = space();
      div1 = element("div");
      input = element("input");
      t4 = space();
      button = element("button");
      button.textContent = "Test";
      attr(div0, "class", "label");
      attr(input, "class", "input");
      set_style(input, "flex", "1");
      attr(input, "type", "text");
      input.value = input_value_value = /*messageForm*/
      ((_b = (_a = ctx[11][
        /*activeSerial*/
        ctx[0]
      ]) == null ? void 0 : _a.cw) == null ? void 0 : _b[
        /*idx*/
        ctx[601]
      ]) ?? "";
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div1, "class", "value");
      set_style(div1, "display", "flex");
      set_style(div1, "gap", "0.4em");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t3);
      append(div2, div1);
      append(div1, input);
      append(div1, t4);
      append(div1, button);
      if (!mounted) {
        dispose = [
          listen(input, "input", input_handler_35),
          listen(button, "click", click_handler_6)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a, _b;
      ctx = new_ctx;
      if (dirty[0] & /*messageForm, activeSerial*/
      2049 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_value_value !== (input_value_value = /*messageForm*/
      ((_b = (_a = ctx[11][
        /*activeSerial*/
        ctx[0]
      ]) == null ? void 0 : _a.cw) == null ? void 0 : _b[
        /*idx*/
        ctx[601]
      ]) ?? "") && input.value !== input_value_value) {
        input.value = input_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_54(ctx) {
  let div;
  let t_value = (
    /*messageStatus*/
    ctx[12][`${/*activeSerial*/
    ctx[0]}:cw`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*messageStatus*/
      ctx[12][`${/*activeSerial*/
      ctx[0]}:cw`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*messageStatus, activeSerial*/
      4097 && t_value !== (t_value = /*messageStatus*/
      ctx2[12][`${/*activeSerial*/
      ctx2[0]}:cw`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*messageStatus, activeSerial*/
      4097 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*messageStatus*/
      ctx2[12][`${/*activeSerial*/
      ctx2[0]}:cw`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_each_block_54(ctx) {
  let div2;
  let div0;
  let t3;
  let div1;
  let input;
  let input_value_value;
  let t4;
  let button;
  let mounted;
  let dispose;
  function input_handler_36(...args) {
    return (
      /*input_handler_36*/
      ctx[323](
        /*idx*/
        ctx[601],
        ...args
      )
    );
  }
  function click_handler_8() {
    return (
      /*click_handler_8*/
      ctx[324](
        /*idx*/
        ctx[601]
      )
    );
  }
  return {
    c() {
      var _a, _b;
      div2 = element("div");
      div0 = element("div");
      div0.textContent = `Message #${/*idx*/
      ctx[601]}:`;
      t3 = space();
      div1 = element("div");
      input = element("input");
      t4 = space();
      button = element("button");
      button.textContent = "Test";
      attr(div0, "class", "label");
      attr(input, "class", "input");
      set_style(input, "flex", "1");
      attr(input, "type", "text");
      input.value = input_value_value = /*messageForm*/
      ((_b = (_a = ctx[11][
        /*activeSerial*/
        ctx[0]
      ]) == null ? void 0 : _a.fsk) == null ? void 0 : _b[
        /*idx*/
        ctx[601]
      ]) ?? "";
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div1, "class", "value");
      set_style(div1, "display", "flex");
      set_style(div1, "gap", "0.4em");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t3);
      append(div2, div1);
      append(div1, input);
      append(div1, t4);
      append(div1, button);
      if (!mounted) {
        dispose = [
          listen(input, "input", input_handler_36),
          listen(button, "click", click_handler_8)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a, _b;
      ctx = new_ctx;
      if (dirty[0] & /*messageForm, activeSerial*/
      2049 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_value_value !== (input_value_value = /*messageForm*/
      ((_b = (_a = ctx[11][
        /*activeSerial*/
        ctx[0]
      ]) == null ? void 0 : _a.fsk) == null ? void 0 : _b[
        /*idx*/
        ctx[601]
      ]) ?? "") && input.value !== input_value_value) {
        input.value = input_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_53(ctx) {
  let div;
  let t_value = (
    /*messageStatus*/
    ctx[12][`${/*activeSerial*/
    ctx[0]}:fsk`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*messageStatus*/
      ctx[12][`${/*activeSerial*/
      ctx[0]}:fsk`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*messageStatus, activeSerial*/
      4097 && t_value !== (t_value = /*messageStatus*/
      ctx2[12][`${/*activeSerial*/
      ctx2[0]}:fsk`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*messageStatus, activeSerial*/
      4097 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*messageStatus*/
      ctx2[12][`${/*activeSerial*/
      ctx2[0]}:fsk`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_51(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Paddle Only Side Tone:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "paddleOnlySideTone"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_98*/
          ctx[300]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "paddleOnlySideTone"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_53(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_52(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_51(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_50(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Paddle Only Side Tone:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "paddleOnlySideTone"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_106*/
          ctx[319]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "paddleOnlySideTone"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_50(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].display + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*opt*/
      ctx2[489].display + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489].value)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_49(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].display + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*opt*/
      ctx2[489].display + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489].value)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_48(ctx) {
  let option;
  let t_value = (
    /*lineOpt*/
    ctx[588].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*lineOpt*/
      ctx[588].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_47(ctx) {
  let div3;
  let div0;
  let t0_value = (
    /*opt*/
    ctx[489].display + ""
  );
  let t0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let t2;
  let div2;
  let select;
  let select_value_value;
  let t3;
  let mounted;
  let dispose;
  function change_handler_96(...args) {
    return (
      /*change_handler_96*/
      ctx[298](
        /*opt*/
        ctx[489],
        ...args
      )
    );
  }
  let each_value_48 = ensure_array_like(
    /*displayLineOptions*/
    ctx[77]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_48.length; i += 1) {
    each_blocks[i] = create_each_block_48(get_each_context_48(ctx, each_value_48, i));
  }
  function change_handler_97(...args) {
    return (
      /*change_handler_97*/
      ctx[299](
        /*opt*/
        ctx[489],
        ...args
      )
    );
  }
  return {
    c() {
      div3 = element("div");
      div0 = element("div");
      t0 = text(t0_value);
      t1 = space();
      div1 = element("div");
      input = element("input");
      t2 = space();
      div2 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t3 = space();
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `dispEv.${/*opt*/
        ctx[489].value}`
      );
      attr(select, "class", "select");
      attr(div3, "class", "table-row");
    },
    m(target, anchor) {
      insert(target, div3, anchor);
      append(div3, div0);
      append(div0, t0);
      append(div3, t1);
      append(div3, div1);
      append(div1, input);
      append(div3, t2);
      append(div3, div2);
      append(div2, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `dispEvLn.${/*opt*/
          ctx[489].value}`
        )
      );
      append(div3, t3);
      if (!mounted) {
        dispose = [
          listen(input, "change", change_handler_96),
          listen(select, "change", change_handler_97)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      ctx = new_ctx;
      if (dirty[0] & /*activeKeyer*/
      4 && t0_value !== (t0_value = /*opt*/
      ctx[489].display + "")) set_data(t0, t0_value);
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[1] & /*activeAntsAndGroups*/
      8192 | dirty[2] & /*displayLineOptions*/
      32768 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `dispEv.${/*opt*/
        ctx[489].value}`
      ))) {
        input.checked = input_checked_value;
      }
      if (dirty[2] & /*displayLineOptions*/
      32768) {
        each_value_48 = ensure_array_like(
          /*displayLineOptions*/
          ctx[77]
        );
        let i;
        for (i = 0; i < each_value_48.length; i += 1) {
          const child_ctx = get_each_context_48(ctx, each_value_48, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_48(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_48.length;
      }
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[1] & /*activeAntsAndGroups*/
      8192 | dirty[2] & /*displayLineOptions*/
      32768 && select_value_value !== (select_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `dispEvLn.${/*opt*/
        ctx[489].value}`
      ))) {
        select_option(
          select,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `dispEvLn.${/*opt*/
            ctx[489].value}`
          )
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div3);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_else_block_2(ctx) {
  let section;
  return {
    c() {
      section = element("section");
      section.innerHTML = `<div class="section-title">Audio</div> <div class="panel"><div class="placeholder">Audio switching is not supported on this device.</div></div>`;
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
      }
    }
  };
}
function create_if_block_47(ctx) {
  let section0;
  let div0;
  let t1;
  let div10;
  let div9;
  let t17;
  let t18;
  let section1;
  let div11;
  let t20;
  let div21;
  let div20;
  let t36;
  let each_value_42 = ensure_array_like(["voice", "digital"]);
  let each_blocks_1 = [];
  for (let i = 0; i < 2; i += 1) {
    each_blocks_1[i] = create_each_block_42(get_each_context_42(ctx, each_value_42, i));
  }
  let each_value_37 = ensure_array_like(["voice", "digital"]);
  let each_blocks = [];
  for (let i = 0; i < 2; i += 1) {
    each_blocks[i] = create_each_block_37(get_each_context_37(ctx, each_value_37, i));
  }
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "Audio Switching Radio 1";
      t1 = space();
      div10 = element("div");
      div9 = element("div");
      div9.innerHTML = `<div>Mode</div> <div>Receive</div> <div>Mic</div> <div>Transmit</div> <div>Mic</div> <div>Transmit with Footswitch</div> <div>Mic</div> <div>Codec</div>`;
      t17 = space();
      for (let i = 0; i < 2; i += 1) {
        each_blocks_1[i].c();
      }
      t18 = space();
      section1 = element("section");
      div11 = element("div");
      div11.textContent = "Audio Switching Radio 2";
      t20 = space();
      div21 = element("div");
      div20 = element("div");
      div20.innerHTML = `<div>Mode</div> <div>Receive</div> <div>Mic</div> <div>Transmit</div> <div>Mic</div> <div>Transmit with Footswitch</div> <div>Mic</div> <div>Codec</div>`;
      t36 = space();
      for (let i = 0; i < 2; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "section-title");
      attr(div9, "class", "table-header");
      attr(div10, "class", "panel table audio-grid-8");
      attr(section0, "class", "section");
      attr(div11, "class", "section-title");
      attr(div20, "class", "table-header");
      attr(div21, "class", "panel table");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div10);
      append(div10, div9);
      append(div10, t17);
      for (let i = 0; i < 2; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(div10, null);
        }
      }
      insert(target, t18, anchor);
      insert(target, section1, anchor);
      append(section1, div11);
      append(section1, t20);
      append(section1, div21);
      append(div21, div20);
      append(div21, t36);
      for (let i = 0; i < 2; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div21, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[2] & /*mk2rCodecOptions*/
      2048 | dirty[3] & /*keyerParam, updateKeyerParam, audioOptionsFor, audioOptionLabel*/
      12300) {
        each_value_42 = ensure_array_like(["voice", "digital"]);
        let i;
        for (i = 0; i < 2; i += 1) {
          const child_ctx = get_each_context_42(ctx2, each_value_42, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_42(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(div10, null);
          }
        }
        for (; i < 2; i += 1) {
          each_blocks_1[i].d(1);
        }
      }
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[2] & /*mk2rCodecOptions*/
      2048 | dirty[3] & /*keyerParam, updateKeyerParam, audioOptionsFor, audioOptionLabel*/
      12300) {
        each_value_37 = ensure_array_like(["voice", "digital"]);
        let i;
        for (i = 0; i < 2; i += 1) {
          const child_ctx = get_each_context_37(ctx2, each_value_37, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_37(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div21, null);
          }
        }
        for (; i < 2; i += 1) {
          each_blocks[i].d(1);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t18);
        detach(section1);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_46(ctx) {
  let section;
  let div0;
  let t1;
  let div6;
  let div5;
  let t9;
  let each_value_32 = ensure_array_like(["voice", "digital"]);
  let each_blocks = [];
  for (let i = 0; i < 2; i += 1) {
    each_blocks[i] = create_each_block_32(get_each_context_32(ctx, each_value_32, i));
  }
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Audio Switching";
      t1 = space();
      div6 = element("div");
      div5 = element("div");
      div5.innerHTML = `<div>Mode</div> <div>Receive</div> <div>Transmit</div> <div>Transmit with Footswitch</div>`;
      t9 = space();
      for (let i = 0; i < 2; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "section-title");
      attr(div5, "class", "table-header");
      attr(div6, "class", "panel table audio-grid-8");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div6);
      append(div6, div5);
      append(div6, t9);
      for (let i = 0; i < 2; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(div6, null);
        }
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[2] & /*mk2rCodecOptions*/
      2048 | dirty[3] & /*keyerParam, updateKeyerParam, audioOptionsFor, audioOptionLabel*/
      12300) {
        each_value_32 = ensure_array_like(["voice", "digital"]);
        let i;
        for (i = 0; i < 2; i += 1) {
          const child_ctx = get_each_context_32(ctx2, each_value_32, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_32(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(div6, null);
          }
        }
        for (; i < 2; i += 1) {
          each_blocks[i].d(1);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block_45(ctx) {
  let section0;
  let div0;
  let t1;
  let div6;
  let div5;
  let t9;
  let t10;
  let section1;
  let div7;
  let t12;
  let div24;
  let div11;
  let t17;
  let div15;
  let div12;
  let t19;
  let div13;
  let input0;
  let input0_checked_value;
  let t20;
  let div14;
  let input1;
  let input1_checked_value;
  let t21;
  let div19;
  let div16;
  let t23;
  let div17;
  let input2;
  let input2_checked_value;
  let t24;
  let div18;
  let input3;
  let input3_checked_value;
  let t25;
  let div23;
  let div20;
  let t27;
  let div21;
  let input4;
  let input4_value_value;
  let t28;
  let div22;
  let input5;
  let input5_value_value;
  let t29;
  let div34;
  let div27;
  let div25;
  let t31;
  let div26;
  let select;
  let select_value_value;
  let t32;
  let div30;
  let div28;
  let t34;
  let div29;
  let input6;
  let input6_checked_value;
  let t35;
  let div33;
  let div31;
  let t37;
  let div32;
  let input7;
  let input7_checked_value;
  let mounted;
  let dispose;
  let each_value_28 = ensure_array_like(["voice", "digital"]);
  let each_blocks_1 = [];
  for (let i = 0; i < 2; i += 1) {
    each_blocks_1[i] = create_each_block_28(get_each_context_28(ctx, each_value_28, i));
  }
  let each_value_27 = ensure_array_like(
    /*mk2MicSelOptions*/
    ctx[72]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_27.length; i += 1) {
    each_blocks[i] = create_each_block_27(get_each_context_27(ctx, each_value_27, i));
  }
  return {
    c() {
      section0 = element("section");
      div0 = element("div");
      div0.textContent = "Audio Switching";
      t1 = space();
      div6 = element("div");
      div5 = element("div");
      div5.innerHTML = `<div>Mode</div> <div>Receive</div> <div>Transmit</div> <div>Transmit with Footswitch</div>`;
      t9 = space();
      for (let i = 0; i < 2; i += 1) {
        each_blocks_1[i].c();
      }
      t10 = space();
      section1 = element("section");
      div7 = element("div");
      div7.textContent = "Audio Options";
      t12 = space();
      div24 = element("div");
      div11 = element("div");
      div11.innerHTML = `<div></div> <div>Voice</div> <div>Digital</div>`;
      t17 = space();
      div15 = element("div");
      div12 = element("div");
      div12.textContent = "Enable On Air Recording";
      t19 = space();
      div13 = element("div");
      input0 = element("input");
      t20 = space();
      div14 = element("div");
      input1 = element("input");
      t21 = space();
      div19 = element("div");
      div16 = element("div");
      div16.textContent = "Recording enabled by Software (Logger)";
      t23 = space();
      div17 = element("div");
      input2 = element("input");
      t24 = space();
      div18 = element("div");
      input3 = element("input");
      t25 = space();
      div23 = element("div");
      div20 = element("div");
      div20.textContent = "Audio Monitor Level (0-25)";
      t27 = space();
      div21 = element("div");
      input4 = element("input");
      t28 = space();
      div22 = element("div");
      input5 = element("input");
      t29 = space();
      div34 = element("div");
      div27 = element("div");
      div25 = element("div");
      div25.textContent = "Microphone Selector:";
      t31 = space();
      div26 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t32 = space();
      div30 = element("div");
      div28 = element("div");
      div28.textContent = "Sound Card PTT:";
      t34 = space();
      div29 = element("div");
      input6 = element("input");
      t35 = space();
      div33 = element("div");
      div31 = element("div");
      div31.textContent = "Downstream over Footswitch:";
      t37 = space();
      div32 = element("div");
      input7 = element("input");
      attr(div0, "class", "section-title");
      attr(div5, "class", "table-header");
      attr(div6, "class", "panel table audio-grid-4");
      attr(section0, "class", "section");
      attr(div7, "class", "section-title");
      attr(div11, "class", "table-header");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FrMpkExtra_Voice.onAirRecActive"
      );
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FrMpkExtra_Digital.onAirRecActive"
      );
      attr(div15, "class", "table-row");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FrMpkExtra_Voice.onAirRecControlByRouter"
      );
      attr(input3, "type", "checkbox");
      input3.checked = input3_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FrMpkExtra_Digital.onAirRecControlByRouter"
      );
      attr(div19, "class", "table-row alt");
      attr(input4, "class", "input");
      attr(input4, "type", "number");
      input4.value = input4_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FrMpkExtra_Voice.pwmMonctr"
      );
      attr(input5, "class", "input");
      attr(input5, "type", "number");
      input5.value = input5_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FrMpkExtra_Digital.pwmMonctr"
      );
      attr(div23, "class", "table-row");
      attr(div24, "class", "panel table audio-grid-3");
      attr(div25, "class", "label");
      attr(select, "class", "select");
      attr(div26, "class", "value");
      attr(div27, "class", "row");
      attr(div28, "class", "label");
      attr(input6, "type", "checkbox");
      input6.checked = input6_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "useAutoPtt"
      );
      attr(div29, "class", "value");
      attr(div30, "class", "row");
      attr(div31, "class", "label");
      attr(input7, "type", "checkbox");
      input7.checked = input7_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "downstreamOverFootSw"
      );
      attr(div32, "class", "value");
      attr(div33, "class", "row");
      attr(div34, "class", "panel");
      set_style(div34, "margin-top", "12px");
      attr(section1, "class", "section");
    },
    m(target, anchor) {
      insert(target, section0, anchor);
      append(section0, div0);
      append(section0, t1);
      append(section0, div6);
      append(div6, div5);
      append(div6, t9);
      for (let i = 0; i < 2; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(div6, null);
        }
      }
      insert(target, t10, anchor);
      insert(target, section1, anchor);
      append(section1, div7);
      append(section1, t12);
      append(section1, div24);
      append(div24, div11);
      append(div24, t17);
      append(div24, div15);
      append(div15, div12);
      append(div15, t19);
      append(div15, div13);
      append(div13, input0);
      append(div15, t20);
      append(div15, div14);
      append(div14, input1);
      append(div24, t21);
      append(div24, div19);
      append(div19, div16);
      append(div19, t23);
      append(div19, div17);
      append(div17, input2);
      append(div19, t24);
      append(div19, div18);
      append(div18, input3);
      append(div24, t25);
      append(div24, div23);
      append(div23, div20);
      append(div23, t27);
      append(div23, div21);
      append(div21, input4);
      append(div23, t28);
      append(div23, div22);
      append(div22, input5);
      append(section1, t29);
      append(section1, div34);
      append(div34, div27);
      append(div27, div25);
      append(div27, t31);
      append(div27, div26);
      append(div26, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*mk2MicSelValue*/
        ctx[97](
          /*activeSerial*/
          ctx[0]
        )
      );
      append(div34, t32);
      append(div34, div30);
      append(div30, div28);
      append(div30, t34);
      append(div30, div29);
      append(div29, input6);
      append(div34, t35);
      append(div34, div33);
      append(div33, div31);
      append(div33, t37);
      append(div33, div32);
      append(div32, input7);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "change",
            /*change_handler_66*/
            ctx[264]
          ),
          listen(
            input1,
            "change",
            /*change_handler_67*/
            ctx[265]
          ),
          listen(
            input2,
            "change",
            /*change_handler_68*/
            ctx[266]
          ),
          listen(
            input3,
            "change",
            /*change_handler_69*/
            ctx[267]
          ),
          listen(
            input4,
            "input",
            /*input_handler_20*/
            ctx[268]
          ),
          listen(
            input5,
            "input",
            /*input_handler_21*/
            ctx[269]
          ),
          listen(
            select,
            "change",
            /*change_handler_70*/
            ctx[270]
          ),
          listen(
            input6,
            "change",
            /*change_handler_71*/
            ctx[271]
          ),
          listen(
            input7,
            "change",
            /*change_handler_72*/
            ctx[272]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial, activeKeyer*/
      5 | dirty[3] & /*keyerParam, updateKeyerParam, audioOptionsFor, audioOptionLabel*/
      12300) {
        each_value_28 = ensure_array_like(["voice", "digital"]);
        let i;
        for (i = 0; i < 2; i += 1) {
          const child_ctx = get_each_context_28(ctx2, each_value_28, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_28(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(div6, null);
          }
        }
        for (; i < 2; i += 1) {
          each_blocks_1[i].d(1);
        }
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FrMpkExtra_Voice.onAirRecActive"
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FrMpkExtra_Digital.onAirRecActive"
      ))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_checked_value !== (input2_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FrMpkExtra_Voice.onAirRecControlByRouter"
      ))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input3_checked_value !== (input3_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FrMpkExtra_Digital.onAirRecControlByRouter"
      ))) {
        input3.checked = input3_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input4_value_value !== (input4_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FrMpkExtra_Voice.pwmMonctr"
      )) && input4.value !== input4_value_value) {
        input4.value = input4_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input5_value_value !== (input5_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FrMpkExtra_Digital.pwmMonctr"
      )) && input5.value !== input5_value_value) {
        input5.value = input5_value_value;
      }
      if (dirty[2] & /*mk2MicSelOptions*/
      1024) {
        each_value_27 = ensure_array_like(
          /*mk2MicSelOptions*/
          ctx2[72]
        );
        let i;
        for (i = 0; i < each_value_27.length; i += 1) {
          const child_ctx = get_each_context_27(ctx2, each_value_27, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_27(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_27.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select_value_value !== (select_value_value = /*mk2MicSelValue*/
      ctx2[97](
        /*activeSerial*/
        ctx2[0]
      ))) {
        select_option(
          select,
          /*mk2MicSelValue*/
          ctx2[97](
            /*activeSerial*/
            ctx2[0]
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input6_checked_value !== (input6_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "useAutoPtt"
      ))) {
        input6.checked = input6_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input7_checked_value !== (input7_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "downstreamOverFootSw"
      ))) {
        input7.checked = input7_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section0);
        detach(t10);
        detach(section1);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_46(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_45(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_44(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_43(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_42(ctx) {
  var _a, _b, _c;
  let div8;
  let div0;
  let t1;
  let div1;
  let select0;
  let select0_value_value;
  let t2;
  let div2;
  let input0;
  let input0_checked_value;
  let t3;
  let div3;
  let select1;
  let select1_value_value;
  let t4;
  let div4;
  let input1;
  let input1_checked_value;
  let t5;
  let div5;
  let select2;
  let select2_value_value;
  let t6;
  let div6;
  let input2;
  let input2_checked_value;
  let t7;
  let div7;
  let select3;
  let select3_value_value;
  let t8;
  let mounted;
  let dispose;
  let each_value_46 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_3 = [];
  for (let i = 0; i < each_value_46.length; i += 1) {
    each_blocks_3[i] = create_each_block_46(get_each_context_46(ctx, each_value_46, i));
  }
  function change_handler_80(...args) {
    return (
      /*change_handler_80*/
      ctx[280](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_81(...args) {
    return (
      /*change_handler_81*/
      ctx[281](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_45 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_45.length; i += 1) {
    each_blocks_2[i] = create_each_block_45(get_each_context_45(ctx, each_value_45, i));
  }
  function change_handler_82(...args) {
    return (
      /*change_handler_82*/
      ctx[282](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_83(...args) {
    return (
      /*change_handler_83*/
      ctx[283](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_44 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_44.length; i += 1) {
    each_blocks_1[i] = create_each_block_44(get_each_context_44(ctx, each_value_44, i));
  }
  function change_handler_84(...args) {
    return (
      /*change_handler_84*/
      ctx[284](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_85(...args) {
    return (
      /*change_handler_85*/
      ctx[285](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_43 = ensure_array_like(
    /*mk2rCodecOptions*/
    ctx[73]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_43.length; i += 1) {
    each_blocks[i] = create_each_block_43(get_each_context_43(ctx, each_value_43, i));
  }
  function change_handler_86(...args) {
    return (
      /*change_handler_86*/
      ctx[286](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  return {
    c() {
      div8 = element("div");
      div0 = element("div");
      div0.textContent = `${/*mode*/
      ctx[547].toUpperCase()}`;
      t1 = space();
      div1 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        each_blocks_3[i].c();
      }
      t2 = space();
      div2 = element("div");
      input0 = element("input");
      t3 = space();
      div3 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      t5 = space();
      div5 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t6 = space();
      div6 = element("div");
      input2 = element("input");
      t7 = space();
      div7 = element("div");
      select3 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t8 = space();
      attr(select0, "class", "select");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRxMic`
      );
      attr(select1, "class", "select");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxMic`
      );
      attr(select2, "class", "select");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSwMic`
      );
      attr(select3, "class", "select");
      attr(div8, "class", "table-row");
    },
    m(target, anchor) {
      insert(target, div8, anchor);
      append(div8, div0);
      append(div8, t1);
      append(div8, div1);
      append(div1, select0);
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        if (each_blocks_3[i]) {
          each_blocks_3[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioRx`
        )
      );
      append(div8, t2);
      append(div8, div2);
      append(div2, input0);
      append(div8, t3);
      append(div8, div3);
      append(div3, select1);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTx`
        )
      );
      append(div8, t4);
      append(div8, div4);
      append(div4, input1);
      append(div8, t5);
      append(div8, div5);
      append(div5, select2);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTxFootSw`
        )
      );
      append(div8, t6);
      append(div8, div6);
      append(div6, input2);
      append(div8, t7);
      append(div8, div7);
      append(div7, select3);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select3, null);
        }
      }
      select_option(
        select3,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrMokExtra_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.voiceCodec`
        )
      );
      append(div8, t8);
      if (!mounted) {
        dispose = [
          listen(select0, "change", change_handler_80),
          listen(input0, "change", change_handler_81),
          listen(select1, "change", change_handler_82),
          listen(input1, "change", change_handler_83),
          listen(select2, "change", change_handler_84),
          listen(input2, "change", change_handler_85),
          listen(select3, "change", change_handler_86)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a2, _b2, _c2;
      ctx = new_ctx;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_46 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_a2 = ctx[2]) == null ? void 0 : _a2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_46.length; i += 1) {
          const child_ctx = get_each_context_46(ctx, each_value_46, i);
          if (each_blocks_3[i]) {
            each_blocks_3[i].p(child_ctx, dirty);
          } else {
            each_blocks_3[i] = create_each_block_46(child_ctx);
            each_blocks_3[i].c();
            each_blocks_3[i].m(select0, null);
          }
        }
        for (; i < each_blocks_3.length; i += 1) {
          each_blocks_3[i].d(1);
        }
        each_blocks_3.length = each_value_46.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRx`
      ))) {
        select_option(
          select0,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioRx`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRxMic`
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_45 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_b2 = ctx[2]) == null ? void 0 : _b2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_45.length; i += 1) {
          const child_ctx = get_each_context_45(ctx, each_value_45, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_45(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select1, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_45.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTx`
      ))) {
        select_option(
          select1,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTx`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxMic`
      ))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_44 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_c2 = ctx[2]) == null ? void 0 : _c2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_44.length; i += 1) {
          const child_ctx = get_each_context_44(ctx, each_value_44, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_44(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select2, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_44.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select2_value_value !== (select2_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSw`
      ))) {
        select_option(
          select2,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTxFootSw`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_checked_value !== (input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSwMic`
      ))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[2] & /*mk2rCodecOptions*/
      2048) {
        each_value_43 = ensure_array_like(
          /*mk2rCodecOptions*/
          ctx[73]
        );
        let i;
        for (i = 0; i < each_value_43.length; i += 1) {
          const child_ctx = get_each_context_43(ctx, each_value_43, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_43(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select3, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_43.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select3_value_value !== (select3_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.voiceCodec`
      ))) {
        select_option(
          select3,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrMokExtra_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.voiceCodec`
          )
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div8);
      }
      destroy_each(each_blocks_3, detaching);
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_41(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_40(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_39(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_38(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_37(ctx) {
  var _a, _b, _c;
  let div8;
  let div0;
  let t1;
  let div1;
  let select0;
  let select0_value_value;
  let t2;
  let div2;
  let input0;
  let input0_checked_value;
  let t3;
  let div3;
  let select1;
  let select1_value_value;
  let t4;
  let div4;
  let input1;
  let input1_checked_value;
  let t5;
  let div5;
  let select2;
  let select2_value_value;
  let t6;
  let div6;
  let input2;
  let input2_checked_value;
  let t7;
  let div7;
  let select3;
  let select3_value_value;
  let t8;
  let mounted;
  let dispose;
  let each_value_41 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_3 = [];
  for (let i = 0; i < each_value_41.length; i += 1) {
    each_blocks_3[i] = create_each_block_41(get_each_context_41(ctx, each_value_41, i));
  }
  function change_handler_87(...args) {
    return (
      /*change_handler_87*/
      ctx[287](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_88(...args) {
    return (
      /*change_handler_88*/
      ctx[288](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_40 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_40.length; i += 1) {
    each_blocks_2[i] = create_each_block_40(get_each_context_40(ctx, each_value_40, i));
  }
  function change_handler_89(...args) {
    return (
      /*change_handler_89*/
      ctx[289](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_90(...args) {
    return (
      /*change_handler_90*/
      ctx[290](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_39 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_39.length; i += 1) {
    each_blocks_1[i] = create_each_block_39(get_each_context_39(ctx, each_value_39, i));
  }
  function change_handler_91(...args) {
    return (
      /*change_handler_91*/
      ctx[291](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_92(...args) {
    return (
      /*change_handler_92*/
      ctx[292](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_38 = ensure_array_like(
    /*mk2rCodecOptions*/
    ctx[73]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_38.length; i += 1) {
    each_blocks[i] = create_each_block_38(get_each_context_38(ctx, each_value_38, i));
  }
  function change_handler_93(...args) {
    return (
      /*change_handler_93*/
      ctx[293](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  return {
    c() {
      div8 = element("div");
      div0 = element("div");
      div0.textContent = `${/*mode*/
      ctx[547].toUpperCase()}`;
      t1 = space();
      div1 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        each_blocks_3[i].c();
      }
      t2 = space();
      div2 = element("div");
      input0 = element("input");
      t3 = space();
      div3 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      t5 = space();
      div5 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t6 = space();
      div6 = element("div");
      input2 = element("input");
      t7 = space();
      div7 = element("div");
      select3 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t8 = space();
      attr(select0, "class", "select");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRxMic`
      );
      attr(select1, "class", "select");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxMic`
      );
      attr(select2, "class", "select");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSwMic`
      );
      attr(select3, "class", "select");
      attr(div8, "class", "table-row");
    },
    m(target, anchor) {
      insert(target, div8, anchor);
      append(div8, div0);
      append(div8, t1);
      append(div8, div1);
      append(div1, select0);
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        if (each_blocks_3[i]) {
          each_blocks_3[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r2FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioRx`
        )
      );
      append(div8, t2);
      append(div8, div2);
      append(div2, input0);
      append(div8, t3);
      append(div8, div3);
      append(div3, select1);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r2FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTx`
        )
      );
      append(div8, t4);
      append(div8, div4);
      append(div4, input1);
      append(div8, t5);
      append(div8, div5);
      append(div5, select2);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r2FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTxFootSw`
        )
      );
      append(div8, t6);
      append(div8, div6);
      append(div6, input2);
      append(div8, t7);
      append(div8, div7);
      append(div7, select3);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select3, null);
        }
      }
      select_option(
        select3,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r2FrMokExtra_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.voiceCodec`
        )
      );
      append(div8, t8);
      if (!mounted) {
        dispose = [
          listen(select0, "change", change_handler_87),
          listen(input0, "change", change_handler_88),
          listen(select1, "change", change_handler_89),
          listen(input1, "change", change_handler_90),
          listen(select2, "change", change_handler_91),
          listen(input2, "change", change_handler_92),
          listen(select3, "change", change_handler_93)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a2, _b2, _c2;
      ctx = new_ctx;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_41 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_a2 = ctx[2]) == null ? void 0 : _a2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_41.length; i += 1) {
          const child_ctx = get_each_context_41(ctx, each_value_41, i);
          if (each_blocks_3[i]) {
            each_blocks_3[i].p(child_ctx, dirty);
          } else {
            each_blocks_3[i] = create_each_block_41(child_ctx);
            each_blocks_3[i].c();
            each_blocks_3[i].m(select0, null);
          }
        }
        for (; i < each_blocks_3.length; i += 1) {
          each_blocks_3[i].d(1);
        }
        each_blocks_3.length = each_value_41.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRx`
      ))) {
        select_option(
          select0,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r2FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioRx`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRxMic`
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_40 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_b2 = ctx[2]) == null ? void 0 : _b2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_40.length; i += 1) {
          const child_ctx = get_each_context_40(ctx, each_value_40, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_40(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select1, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_40.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTx`
      ))) {
        select_option(
          select1,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r2FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTx`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxMic`
      ))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_39 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_c2 = ctx[2]) == null ? void 0 : _c2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_39.length; i += 1) {
          const child_ctx = get_each_context_39(ctx, each_value_39, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_39(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select2, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_39.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select2_value_value !== (select2_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSw`
      ))) {
        select_option(
          select2,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r2FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTxFootSw`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_checked_value !== (input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSwMic`
      ))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[2] & /*mk2rCodecOptions*/
      2048) {
        each_value_38 = ensure_array_like(
          /*mk2rCodecOptions*/
          ctx[73]
        );
        let i;
        for (i = 0; i < each_value_38.length; i += 1) {
          const child_ctx = get_each_context_38(ctx, each_value_38, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_38(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select3, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_38.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select3_value_value !== (select3_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r2FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.voiceCodec`
      ))) {
        select_option(
          select3,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r2FrMokExtra_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.voiceCodec`
          )
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div8);
      }
      destroy_each(each_blocks_3, detaching);
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_36(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_35(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_34(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_33(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_32(ctx) {
  var _a, _b, _c;
  let div8;
  let div0;
  let t1;
  let div1;
  let select0;
  let select0_value_value;
  let t2;
  let div2;
  let input0;
  let input0_checked_value;
  let t3;
  let div3;
  let select1;
  let select1_value_value;
  let t4;
  let div4;
  let input1;
  let input1_checked_value;
  let t5;
  let div5;
  let select2;
  let select2_value_value;
  let t6;
  let div6;
  let input2;
  let input2_checked_value;
  let t7;
  let div7;
  let select3;
  let select3_value_value;
  let t8;
  let mounted;
  let dispose;
  let each_value_36 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_3 = [];
  for (let i = 0; i < each_value_36.length; i += 1) {
    each_blocks_3[i] = create_each_block_36(get_each_context_36(ctx, each_value_36, i));
  }
  function change_handler_73(...args) {
    return (
      /*change_handler_73*/
      ctx[273](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_74(...args) {
    return (
      /*change_handler_74*/
      ctx[274](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_35 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_35.length; i += 1) {
    each_blocks_2[i] = create_each_block_35(get_each_context_35(ctx, each_value_35, i));
  }
  function change_handler_75(...args) {
    return (
      /*change_handler_75*/
      ctx[275](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_76(...args) {
    return (
      /*change_handler_76*/
      ctx[276](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_34 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_34.length; i += 1) {
    each_blocks_1[i] = create_each_block_34(get_each_context_34(ctx, each_value_34, i));
  }
  function change_handler_77(...args) {
    return (
      /*change_handler_77*/
      ctx[277](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  function change_handler_78(...args) {
    return (
      /*change_handler_78*/
      ctx[278](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_33 = ensure_array_like(
    /*mk2rCodecOptions*/
    ctx[73]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_33.length; i += 1) {
    each_blocks[i] = create_each_block_33(get_each_context_33(ctx, each_value_33, i));
  }
  function change_handler_79(...args) {
    return (
      /*change_handler_79*/
      ctx[279](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  return {
    c() {
      div8 = element("div");
      div0 = element("div");
      div0.textContent = `${/*mode*/
      ctx[547].toUpperCase()}`;
      t1 = space();
      div1 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        each_blocks_3[i].c();
      }
      t2 = space();
      div2 = element("div");
      input0 = element("input");
      t3 = space();
      div3 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      t5 = space();
      div5 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t6 = space();
      div6 = element("div");
      input2 = element("input");
      t7 = space();
      div7 = element("div");
      select3 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t8 = space();
      attr(select0, "class", "select");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRxMic`
      );
      attr(select1, "class", "select");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxMic`
      );
      attr(select2, "class", "select");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSwMic`
      );
      attr(select3, "class", "select");
      attr(div8, "class", "table-row");
    },
    m(target, anchor) {
      insert(target, div8, anchor);
      append(div8, div0);
      append(div8, t1);
      append(div8, div1);
      append(div1, select0);
      for (let i = 0; i < each_blocks_3.length; i += 1) {
        if (each_blocks_3[i]) {
          each_blocks_3[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioRx`
        )
      );
      append(div8, t2);
      append(div8, div2);
      append(div2, input0);
      append(div8, t3);
      append(div8, div3);
      append(div3, select1);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTx`
        )
      );
      append(div8, t4);
      append(div8, div4);
      append(div4, input1);
      append(div8, t5);
      append(div8, div5);
      append(div5, select2);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTxFootSw`
        )
      );
      append(div8, t6);
      append(div8, div6);
      append(div6, input2);
      append(div8, t7);
      append(div8, div7);
      append(div7, select3);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select3, null);
        }
      }
      select_option(
        select3,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrMokExtra_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.voiceCodec`
        )
      );
      append(div8, t8);
      if (!mounted) {
        dispose = [
          listen(select0, "change", change_handler_73),
          listen(input0, "change", change_handler_74),
          listen(select1, "change", change_handler_75),
          listen(input1, "change", change_handler_76),
          listen(select2, "change", change_handler_77),
          listen(input2, "change", change_handler_78),
          listen(select3, "change", change_handler_79)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a2, _b2, _c2;
      ctx = new_ctx;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_36 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_a2 = ctx[2]) == null ? void 0 : _a2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_36.length; i += 1) {
          const child_ctx = get_each_context_36(ctx, each_value_36, i);
          if (each_blocks_3[i]) {
            each_blocks_3[i].p(child_ctx, dirty);
          } else {
            each_blocks_3[i] = create_each_block_36(child_ctx);
            each_blocks_3[i].c();
            each_blocks_3[i].m(select0, null);
          }
        }
        for (; i < each_blocks_3.length; i += 1) {
          each_blocks_3[i].d(1);
        }
        each_blocks_3.length = each_value_36.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRx`
      ))) {
        select_option(
          select0,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioRx`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRxMic`
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_35 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_b2 = ctx[2]) == null ? void 0 : _b2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_35.length; i += 1) {
          const child_ctx = get_each_context_35(ctx, each_value_35, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_35(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select1, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_35.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTx`
      ))) {
        select_option(
          select1,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTx`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxMic`
      ))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_34 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_c2 = ctx[2]) == null ? void 0 : _c2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_34.length; i += 1) {
          const child_ctx = get_each_context_34(ctx, each_value_34, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_34(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select2, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_34.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select2_value_value !== (select2_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSw`
      ))) {
        select_option(
          select2,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTxFootSw`
          )
        );
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input2_checked_value !== (input2_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSwMic`
      ))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[2] & /*mk2rCodecOptions*/
      2048) {
        each_value_33 = ensure_array_like(
          /*mk2rCodecOptions*/
          ctx[73]
        );
        let i;
        for (i = 0; i < each_value_33.length; i += 1) {
          const child_ctx = get_each_context_33(ctx, each_value_33, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_33(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select3, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_33.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select3_value_value !== (select3_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrMokExtra_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.voiceCodec`
      ))) {
        select_option(
          select3,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrMokExtra_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.voiceCodec`
          )
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div8);
      }
      destroy_each(each_blocks_3, detaching);
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_31(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_30(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_29(ctx) {
  let option;
  let t_value = (
    /*audioOptionLabel*/
    ctx[96](
      /*opt*/
      ctx[489]
    ) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*audioOptionLabel*/
      ctx2[96](
        /*opt*/
        ctx2[489]
      ) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_28(ctx) {
  var _a, _b, _c;
  let div4;
  let div0;
  let t1;
  let div1;
  let select0;
  let select0_value_value;
  let t2;
  let div2;
  let select1;
  let select1_value_value;
  let t3;
  let div3;
  let select2;
  let select2_value_value;
  let t4;
  let mounted;
  let dispose;
  let each_value_31 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_31.length; i += 1) {
    each_blocks_2[i] = create_each_block_31(get_each_context_31(ctx, each_value_31, i));
  }
  function change_handler_63(...args) {
    return (
      /*change_handler_63*/
      ctx[261](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_30 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_30.length; i += 1) {
    each_blocks_1[i] = create_each_block_30(get_each_context_30(ctx, each_value_30, i));
  }
  function change_handler_64(...args) {
    return (
      /*change_handler_64*/
      ctx[262](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  let each_value_29 = ensure_array_like(
    /*audioOptionsFor*/
    ctx[95](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0,
      /*mode*/
      ctx[547]
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_29.length; i += 1) {
    each_blocks[i] = create_each_block_29(get_each_context_29(ctx, each_value_29, i));
  }
  function change_handler_65(...args) {
    return (
      /*change_handler_65*/
      ctx[263](
        /*mode*/
        ctx[547],
        ...args
      )
    );
  }
  return {
    c() {
      div4 = element("div");
      div0 = element("div");
      div0.textContent = `${/*mode*/
      ctx[547].toUpperCase()}`;
      t1 = space();
      div1 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t2 = space();
      div2 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t3 = space();
      div3 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t4 = space();
      attr(select0, "class", "select");
      attr(select1, "class", "select");
      attr(select2, "class", "select");
      attr(div4, "class", "table-row");
    },
    m(target, anchor) {
      insert(target, div4, anchor);
      append(div4, div0);
      append(div4, t1);
      append(div4, div1);
      append(div1, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioRx`
        )
      );
      append(div4, t2);
      append(div4, div2);
      append(div2, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTx`
        )
      );
      append(div4, t3);
      append(div4, div3);
      append(div3, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          `r1FrBase_${/*mode*/
          ctx[547] === "cw" ? "Cw" : (
            /*mode*/
            ctx[547] === "voice" ? "Voice" : "Digital"
          )}.audioTxFootSw`
        )
      );
      append(div4, t4);
      if (!mounted) {
        dispose = [
          listen(select0, "change", change_handler_63),
          listen(select1, "change", change_handler_64),
          listen(select2, "change", change_handler_65)
        ];
        mounted = true;
      }
    },
    p(new_ctx, dirty) {
      var _a2, _b2, _c2;
      ctx = new_ctx;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_31 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_a2 = ctx[2]) == null ? void 0 : _a2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_31.length; i += 1) {
          const child_ctx = get_each_context_31(ctx, each_value_31, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_31(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_31.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select0_value_value !== (select0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioRx`
      ))) {
        select_option(
          select0,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioRx`
          )
        );
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_30 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_b2 = ctx[2]) == null ? void 0 : _b2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_30.length; i += 1) {
          const child_ctx = get_each_context_30(ctx, each_value_30, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_30(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_30.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select1_value_value !== (select1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTx`
      ))) {
        select_option(
          select1,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTx`
          )
        );
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[3] & /*audioOptionsFor, audioOptionLabel*/
      12) {
        each_value_29 = ensure_array_like(
          /*audioOptionsFor*/
          ctx[95](
            /*activeKeyer*/
            ((_c2 = ctx[2]) == null ? void 0 : _c2.type) ?? 0,
            /*mode*/
            ctx[547]
          )
        );
        let i;
        for (i = 0; i < each_value_29.length; i += 1) {
          const child_ctx = get_each_context_29(ctx, each_value_29, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_29(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_29.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select2_value_value !== (select2_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        `r1FrBase_${/*mode*/
        ctx[547] === "cw" ? "Cw" : (
          /*mode*/
          ctx[547] === "voice" ? "Voice" : "Digital"
        )}.audioTxFootSw`
      ))) {
        select_option(
          select2,
          /*keyerParam*/
          ctx[105](
            /*activeSerial*/
            ctx[0],
            `r1FrBase_${/*mode*/
            ctx[547] === "cw" ? "Cw" : (
              /*mode*/
              ctx[547] === "voice" ? "Voice" : "Digital"
            )}.audioTxFootSw`
          )
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div4);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_27(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_26(ctx) {
  let option;
  let t_value = (
    /*pttOptionLabels*/
    (ctx[79][
      /*opt*/
      ctx[489]
    ] || /*opt*/
    ctx[489]) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*pttOptionLabels*/
      (ctx2[79][
        /*opt*/
        ctx2[489]
      ] || /*opt*/
      ctx2[489]) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_43(ctx) {
  var _a;
  let div2;
  let div0;
  let t1;
  let div1;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  let each_value_25 = ensure_array_like(
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      "voice"
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_25.length; i += 1) {
    each_blocks[i] = create_each_block_25(get_each_context_25(ctx, each_value_25, i));
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "PTT Voice:";
      t1 = space();
      div1 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "label");
      attr(select, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      var _a2;
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*activePttR1*/
        (_a2 = ctx[37]) == null ? void 0 : _a2.voice
      );
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*change_handler_45*/
          ctx[237]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b, _c;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*pttOptionLabels*/
      131072 | dirty[3] & /*pttOptionsFor*/
      128) {
        each_value_25 = ensure_array_like(
          /*pttOptionsFor*/
          ctx2[100](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0,
            "voice"
          )
        );
        let i;
        for (i = 0; i < each_value_25.length; i += 1) {
          const child_ctx = get_each_context_25(ctx2, each_value_25, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_25(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_25.length;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[1] & /*activePttR1*/
      64 | dirty[3] & /*pttOptionsFor*/
      128 && select_value_value !== (select_value_value = /*activePttR1*/
      (_b = ctx2[37]) == null ? void 0 : _b.voice)) {
        select_option(
          select,
          /*activePttR1*/
          (_c = ctx2[37]) == null ? void 0 : _c.voice
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_25(ctx) {
  let option;
  let t_value = (
    /*pttOptionLabels*/
    (ctx[79][
      /*opt*/
      ctx[489]
    ] || /*opt*/
    ctx[489]) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*pttOptionLabels*/
      (ctx2[79][
        /*opt*/
        ctx2[489]
      ] || /*opt*/
      ctx2[489]) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_42(ctx) {
  var _a;
  let div2;
  let div0;
  let t1;
  let div1;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  let each_value_24 = ensure_array_like(
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      "digital"
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_24.length; i += 1) {
    each_blocks[i] = create_each_block_24(get_each_context_24(ctx, each_value_24, i));
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "PTT Digital:";
      t1 = space();
      div1 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "label");
      attr(select, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      var _a2;
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*activePttR1*/
        (_a2 = ctx[37]) == null ? void 0 : _a2.digital
      );
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*change_handler_46*/
          ctx[238]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b, _c;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*pttOptionLabels*/
      131072 | dirty[3] & /*pttOptionsFor*/
      128) {
        each_value_24 = ensure_array_like(
          /*pttOptionsFor*/
          ctx2[100](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0,
            "digital"
          )
        );
        let i;
        for (i = 0; i < each_value_24.length; i += 1) {
          const child_ctx = get_each_context_24(ctx2, each_value_24, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_24(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_24.length;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[1] & /*activePttR1*/
      64 | dirty[3] & /*pttOptionsFor*/
      128 && select_value_value !== (select_value_value = /*activePttR1*/
      (_b = ctx2[37]) == null ? void 0 : _b.digital)) {
        select_option(
          select,
          /*activePttR1*/
          (_c = ctx2[37]) == null ? void 0 : _c.digital
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_24(ctx) {
  let option;
  let t_value = (
    /*pttOptionLabels*/
    (ctx[79][
      /*opt*/
      ctx[489]
    ] || /*opt*/
    ctx[489]) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*pttOptionLabels*/
      (ctx2[79][
        /*opt*/
        ctx2[489]
      ] || /*opt*/
      ctx2[489]) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_41(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input0;
  let input0_checked_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input1;
  let input1_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "LNA PTT:";
      t1 = space();
      div1 = element("div");
      input0 = element("input");
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "PA PTT:";
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      attr(div0, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1LnaPtt"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1PaPtt"
      );
      attr(div4, "class", "value");
      attr(div5, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input0);
      insert(target, t2, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input1);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "change",
            /*change_handler_47*/
            ctx[239]
          ),
          listen(
            input1,
            "change",
            /*change_handler_48*/
            ctx[240]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1LnaPtt"
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1PaPtt"
      ))) {
        input1.checked = input1_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t2);
        detach(div5);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_40(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input0;
  let input0_value_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input1;
  let input1_value_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "PA PTT Tail (x 10ms):";
      t1 = space();
      div1 = element("div");
      input0 = element("input");
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "LNA PTT Tail (x 10ms):";
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      attr(div0, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1PaPttTail"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      input1.value = input1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1LnaPttTail"
      );
      attr(div4, "class", "value");
      attr(div5, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input0);
      insert(target, t2, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input1);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input_handler_14*/
            ctx[241]
          ),
          listen(
            input1,
            "input",
            /*input_handler_15*/
            ctx[242]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1PaPttTail"
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_value_value !== (input1_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1LnaPttTail"
      )) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t2);
        detach(div5);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_39(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "CW in Voice:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1AllowCwInVoice"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_49*/
          ctx[244]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1AllowCwInVoice"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_38(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input0;
  let input0_checked_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input1;
  let input1_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Sound Card PTT:";
      t1 = space();
      div1 = element("div");
      input0 = element("input");
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "Downstream over Footswitch:";
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      attr(div0, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "useAutoPtt"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "downstreamOverFootSw"
      );
      attr(div4, "class", "value");
      attr(div5, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input0);
      insert(target, t2, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input1);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "change",
            /*change_handler_50*/
            ctx[245]
          ),
          listen(
            input1,
            "change",
            /*change_handler_51*/
            ctx[246]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "useAutoPtt"
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "downstreamOverFootSw"
      ))) {
        input1.checked = input1_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t2);
        detach(div5);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_37(ctx) {
  let div;
  let t_value = (
    /*pttStatus*/
    ctx[10][`${/*activeSerial*/
    ctx[0]}:r1`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*pttStatus*/
      ctx[10][`${/*activeSerial*/
      ctx[0]}:r1`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*pttStatus, activeSerial*/
      1025 && t_value !== (t_value = /*pttStatus*/
      ctx2[10][`${/*activeSerial*/
      ctx2[0]}:r1`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*pttStatus, activeSerial*/
      1025 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*pttStatus*/
      ctx2[10][`${/*activeSerial*/
      ctx2[0]}:r1`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_30(ctx) {
  var _a, _b, _c;
  let section;
  let div0;
  let t1;
  let div7;
  let div3;
  let div1;
  let t3;
  let div2;
  let select;
  let select_value_value;
  let t4;
  let show_if_1 = (
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      "voice"
    ).length
  );
  let t5;
  let show_if = (
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_b = ctx[2]) == null ? void 0 : _b.type) ?? 0,
      "digital"
    ).length
  );
  let t6;
  let t7;
  let t8;
  let div6;
  let div4;
  let t10;
  let div5;
  let input;
  let input_value_value;
  let t11;
  let t12;
  let mounted;
  let dispose;
  let each_value_23 = ensure_array_like(
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_c = ctx[2]) == null ? void 0 : _c.type) ?? 0,
      "cw"
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_23.length; i += 1) {
    each_blocks[i] = create_each_block_23(get_each_context_23(ctx, each_value_23, i));
  }
  let if_block0 = show_if_1 && create_if_block_36(ctx);
  let if_block1 = show_if && create_if_block_35(ctx);
  let if_block2 = (
    /*hasLnaPaPtt*/
    ctx[60] && create_if_block_34(ctx)
  );
  let if_block3 = (
    /*hasLnaPaPttTail*/
    ctx[59] && create_if_block_33(ctx)
  );
  let if_block4 = (
    /*hasCwInVoice*/
    ctx[57] && create_if_block_32(ctx)
  );
  let if_block5 = (
    /*pttStatus*/
    ctx[10][`${/*activeSerial*/
    ctx[0]}:r2`] && create_if_block_31(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "PTT Radio 2";
      t1 = space();
      div7 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "PTT CW:";
      t3 = space();
      div2 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t4 = space();
      if (if_block0) if_block0.c();
      t5 = space();
      if (if_block1) if_block1.c();
      t6 = space();
      if (if_block2) if_block2.c();
      t7 = space();
      if (if_block3) if_block3.c();
      t8 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "PTT Lead (x 10ms):";
      t10 = space();
      div5 = element("div");
      input = element("input");
      t11 = space();
      if (if_block4) if_block4.c();
      t12 = space();
      if (if_block5) if_block5.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(input, "class", "input");
      attr(input, "type", "number");
      input.value = input_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2PttDelay"
      );
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a2;
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div7);
      append(div7, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*activePttR2*/
        (_a2 = ctx[36]) == null ? void 0 : _a2.cw
      );
      append(div7, t4);
      if (if_block0) if_block0.m(div7, null);
      append(div7, t5);
      if (if_block1) if_block1.m(div7, null);
      append(div7, t6);
      if (if_block2) if_block2.m(div7, null);
      append(div7, t7);
      if (if_block3) if_block3.m(div7, null);
      append(div7, t8);
      append(div7, div6);
      append(div6, div4);
      append(div6, t10);
      append(div6, div5);
      append(div5, input);
      append(div7, t11);
      if (if_block4) if_block4.m(div7, null);
      append(section, t12);
      if (if_block5) if_block5.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            select,
            "change",
            /*change_handler_52*/
            ctx[247]
          ),
          listen(
            input,
            "input",
            /*input_handler_19*/
            ctx[254]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c2, _d, _e;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*pttOptionLabels*/
      131072 | dirty[3] & /*pttOptionsFor*/
      128) {
        each_value_23 = ensure_array_like(
          /*pttOptionsFor*/
          ctx2[100](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0,
            "cw"
          )
        );
        let i;
        for (i = 0; i < each_value_23.length; i += 1) {
          const child_ctx = get_each_context_23(ctx2, each_value_23, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_23(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_23.length;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[1] & /*activePttR2*/
      32 | dirty[3] & /*pttOptionsFor*/
      128 && select_value_value !== (select_value_value = /*activePttR2*/
      (_b2 = ctx2[36]) == null ? void 0 : _b2.cw)) {
        select_option(
          select,
          /*activePttR2*/
          (_c2 = ctx2[36]) == null ? void 0 : _c2.cw
        );
      }
      if (dirty[0] & /*activeKeyer*/
      4) show_if_1 = /*pttOptionsFor*/
      ctx2[100](
        /*activeKeyer*/
        ((_d = ctx2[2]) == null ? void 0 : _d.type) ?? 0,
        "voice"
      ).length;
      if (show_if_1) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_36(ctx2);
          if_block0.c();
          if_block0.m(div7, t5);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[0] & /*activeKeyer*/
      4) show_if = /*pttOptionsFor*/
      ctx2[100](
        /*activeKeyer*/
        ((_e = ctx2[2]) == null ? void 0 : _e.type) ?? 0,
        "digital"
      ).length;
      if (show_if) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_35(ctx2);
          if_block1.c();
          if_block1.m(div7, t6);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (
        /*hasLnaPaPtt*/
        ctx2[60]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_34(ctx2);
          if_block2.c();
          if_block2.m(div7, t7);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
      if (
        /*hasLnaPaPttTail*/
        ctx2[59]
      ) {
        if (if_block3) {
          if_block3.p(ctx2, dirty);
        } else {
          if_block3 = create_if_block_33(ctx2);
          if_block3.c();
          if_block3.m(div7, t8);
        }
      } else if (if_block3) {
        if_block3.d(1);
        if_block3 = null;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_value_value !== (input_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2PttDelay"
      )) && input.value !== input_value_value) {
        input.value = input_value_value;
      }
      if (
        /*hasCwInVoice*/
        ctx2[57]
      ) {
        if (if_block4) {
          if_block4.p(ctx2, dirty);
        } else {
          if_block4 = create_if_block_32(ctx2);
          if_block4.c();
          if_block4.m(div7, null);
        }
      } else if (if_block4) {
        if_block4.d(1);
        if_block4 = null;
      }
      if (
        /*pttStatus*/
        ctx2[10][`${/*activeSerial*/
        ctx2[0]}:r2`]
      ) {
        if (if_block5) {
          if_block5.p(ctx2, dirty);
        } else {
          if_block5 = create_if_block_31(ctx2);
          if_block5.c();
          if_block5.m(section, null);
        }
      } else if (if_block5) {
        if_block5.d(1);
        if_block5 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      if (if_block2) if_block2.d();
      if (if_block3) if_block3.d();
      if (if_block4) if_block4.d();
      if (if_block5) if_block5.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_23(ctx) {
  let option;
  let t_value = (
    /*pttOptionLabels*/
    (ctx[79][
      /*opt*/
      ctx[489]
    ] || /*opt*/
    ctx[489]) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*pttOptionLabels*/
      (ctx2[79][
        /*opt*/
        ctx2[489]
      ] || /*opt*/
      ctx2[489]) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_36(ctx) {
  var _a;
  let div2;
  let div0;
  let t1;
  let div1;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  let each_value_22 = ensure_array_like(
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      "voice"
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_22.length; i += 1) {
    each_blocks[i] = create_each_block_22(get_each_context_22(ctx, each_value_22, i));
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "PTT Voice:";
      t1 = space();
      div1 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "label");
      attr(select, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      var _a2;
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*activePttR2*/
        (_a2 = ctx[36]) == null ? void 0 : _a2.voice
      );
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*change_handler_53*/
          ctx[248]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b, _c;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*pttOptionLabels*/
      131072 | dirty[3] & /*pttOptionsFor*/
      128) {
        each_value_22 = ensure_array_like(
          /*pttOptionsFor*/
          ctx2[100](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0,
            "voice"
          )
        );
        let i;
        for (i = 0; i < each_value_22.length; i += 1) {
          const child_ctx = get_each_context_22(ctx2, each_value_22, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_22(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_22.length;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[1] & /*activePttR2*/
      32 | dirty[3] & /*pttOptionsFor*/
      128 && select_value_value !== (select_value_value = /*activePttR2*/
      (_b = ctx2[36]) == null ? void 0 : _b.voice)) {
        select_option(
          select,
          /*activePttR2*/
          (_c = ctx2[36]) == null ? void 0 : _c.voice
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_22(ctx) {
  let option;
  let t_value = (
    /*pttOptionLabels*/
    (ctx[79][
      /*opt*/
      ctx[489]
    ] || /*opt*/
    ctx[489]) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*pttOptionLabels*/
      (ctx2[79][
        /*opt*/
        ctx2[489]
      ] || /*opt*/
      ctx2[489]) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_35(ctx) {
  var _a;
  let div2;
  let div0;
  let t1;
  let div1;
  let select;
  let select_value_value;
  let mounted;
  let dispose;
  let each_value_21 = ensure_array_like(
    /*pttOptionsFor*/
    ctx[100](
      /*activeKeyer*/
      ((_a = ctx[2]) == null ? void 0 : _a.type) ?? 0,
      "digital"
    )
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_21.length; i += 1) {
    each_blocks[i] = create_each_block_21(get_each_context_21(ctx, each_value_21, i));
  }
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "PTT Digital:";
      t1 = space();
      div1 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "label");
      attr(select, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      var _a2;
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*activePttR2*/
        (_a2 = ctx[36]) == null ? void 0 : _a2.digital
      );
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*change_handler_54*/
          ctx[249]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b, _c;
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*pttOptionLabels*/
      131072 | dirty[3] & /*pttOptionsFor*/
      128) {
        each_value_21 = ensure_array_like(
          /*pttOptionsFor*/
          ctx2[100](
            /*activeKeyer*/
            ((_a2 = ctx2[2]) == null ? void 0 : _a2.type) ?? 0,
            "digital"
          )
        );
        let i;
        for (i = 0; i < each_value_21.length; i += 1) {
          const child_ctx = get_each_context_21(ctx2, each_value_21, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_21(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_21.length;
      }
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[1] & /*activePttR2*/
      32 | dirty[3] & /*pttOptionsFor*/
      128 && select_value_value !== (select_value_value = /*activePttR2*/
      (_b = ctx2[36]) == null ? void 0 : _b.digital)) {
        select_option(
          select,
          /*activePttR2*/
          (_c = ctx2[36]) == null ? void 0 : _c.digital
        );
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      destroy_each(each_blocks, detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_21(ctx) {
  let option;
  let t_value = (
    /*pttOptionLabels*/
    (ctx[79][
      /*opt*/
      ctx[489]
    ] || /*opt*/
    ctx[489]) + ""
  );
  let t;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = option_value_value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeKeyer*/
      4 && t_value !== (t_value = /*pttOptionLabels*/
      (ctx2[79][
        /*opt*/
        ctx2[489]
      ] || /*opt*/
      ctx2[489]) + "")) set_data(t, t_value);
      if (dirty[0] & /*activeKeyer*/
      4 | dirty[2] & /*displayLineOptions*/
      32768 && option_value_value !== (option_value_value = /*opt*/
      ctx2[489])) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_34(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input0;
  let input0_checked_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input1;
  let input1_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "LNA PTT:";
      t1 = space();
      div1 = element("div");
      input0 = element("input");
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "PA PTT:";
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      attr(div0, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2LnaPtt"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2PaPtt"
      );
      attr(div4, "class", "value");
      attr(div5, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input0);
      insert(target, t2, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input1);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "change",
            /*change_handler_55*/
            ctx[250]
          ),
          listen(
            input1,
            "change",
            /*change_handler_56*/
            ctx[251]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_checked_value !== (input0_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2LnaPtt"
      ))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2PaPtt"
      ))) {
        input1.checked = input1_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t2);
        detach(div5);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_33(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input0;
  let input0_value_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input1;
  let input1_value_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "PA PTT Tail (x 10ms):";
      t1 = space();
      div1 = element("div");
      input0 = element("input");
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "LNA PTT Tail (x 10ms):";
      t4 = space();
      div4 = element("div");
      input1 = element("input");
      attr(div0, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2PaPttTail"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input1, "class", "input");
      attr(input1, "type", "number");
      input1.value = input1_value_value = /*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2LnaPttTail"
      );
      attr(div4, "class", "value");
      attr(div5, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input0);
      insert(target, t2, anchor);
      insert(target, div5, anchor);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input1);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "input",
            /*input_handler_17*/
            ctx[252]
          ),
          listen(
            input1,
            "input",
            /*input_handler_18*/
            ctx[253]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input0_value_value !== (input0_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2PaPttTail"
      )) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_value_value !== (input1_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2LnaPttTail"
      )) && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
        detach(t2);
        detach(div5);
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_32(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "CW in Voice:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2AllowCwInVoice"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_57*/
          ctx[255]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2AllowCwInVoice"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_31(ctx) {
  let div;
  let t_value = (
    /*pttStatus*/
    ctx[10][`${/*activeSerial*/
    ctx[0]}:r2`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*pttStatus*/
      ctx[10][`${/*activeSerial*/
      ctx[0]}:r2`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*pttStatus, activeSerial*/
      1025 && t_value !== (t_value = /*pttStatus*/
      ctx2[10][`${/*activeSerial*/
      ctx2[0]}:r2`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*pttStatus, activeSerial*/
      1025 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*pttStatus*/
      ctx2[10][`${/*activeSerial*/
      ctx2[0]}:r2`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_26(ctx) {
  let section;
  let div0;
  let t1;
  let div16;
  let div3;
  let div1;
  let t3;
  let div2;
  let select0;
  let select0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let select1;
  let select1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let select2;
  let select2_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let input0;
  let input0_checked_value;
  let t13;
  let div15;
  let div13;
  let t15;
  let div14;
  let input1;
  let input1_checked_value;
  let t16;
  let t17;
  let div17;
  let button;
  let t19;
  let mounted;
  let dispose;
  let each_value_20 = ensure_array_like(
    /*fskBaudOptions*/
    ctx[69]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_20.length; i += 1) {
    each_blocks_2[i] = create_each_block_20(get_each_context_20(ctx, each_value_20, i));
  }
  let each_value_19 = ensure_array_like(
    /*fskDataBitsOptions*/
    ctx[70]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_19.length; i += 1) {
    each_blocks_1[i] = create_each_block_19(get_each_context_19(ctx, each_value_19, i));
  }
  let each_value_18 = ensure_array_like(
    /*fskStopBitsOptions*/
    ctx[71]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_18.length; i += 1) {
    each_blocks[i] = create_each_block_18(get_each_context_18(ctx, each_value_18, i));
  }
  let if_block0 = (
    /*hasPfsk*/
    ctx[62] && create_if_block_28(ctx)
  );
  let if_block1 = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:fsk1`] && create_if_block_27(ctx)
  );
  return {
    c() {
      var _a;
      section = element("section");
      div0 = element("div");
      div0.textContent = "FSK 1";
      t1 = space();
      div16 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Baud Rate:";
      t3 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Data Bits:";
      t6 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Stop Bits:";
      t9 = space();
      div8 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "RTS/CTS Handshake:";
      t12 = space();
      div11 = element("div");
      input0 = element("input");
      t13 = space();
      div15 = element("div");
      div13 = element("div");
      div13.textContent = "Invert FSK:";
      t15 = space();
      div14 = element("div");
      input1 = element("input");
      t16 = space();
      if (if_block0) if_block0.c();
      t17 = space();
      div17 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t19 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(select2, "class", "select");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*activeFsk1*/
      ((_a = ctx[40]) == null ? void 0 : _a.rtscts);
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1InvertFsk"
      );
      attr(div14, "class", "value");
      attr(div15, "class", "row");
      attr(div16, "class", "panel");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div17, "class", "button-row");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a, _b, _c;
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div16);
      append(div16, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeFsk1*/
        (_a = ctx[40]) == null ? void 0 : _a.baud
      );
      append(div16, t4);
      append(div16, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeFsk1*/
        (_b = ctx[40]) == null ? void 0 : _b.databits
      );
      append(div16, t7);
      append(div16, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*activeFsk1*/
        (_c = ctx[40]) == null ? void 0 : _c.stopbits
      );
      append(div16, t10);
      append(div16, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, input0);
      append(div16, t13);
      append(div16, div15);
      append(div15, div13);
      append(div15, t15);
      append(div15, div14);
      append(div14, input1);
      append(div16, t16);
      if (if_block0) if_block0.m(div16, null);
      append(section, t17);
      append(section, div17);
      append(div17, button);
      append(section, t19);
      if (if_block1) if_block1.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_32*/
            ctx[222]
          ),
          listen(
            select1,
            "change",
            /*change_handler_33*/
            ctx[223]
          ),
          listen(
            select2,
            "change",
            /*change_handler_34*/
            ctx[224]
          ),
          listen(
            input0,
            "change",
            /*change_handler_35*/
            ctx[225]
          ),
          listen(
            input1,
            "change",
            /*change_handler_36*/
            ctx[226]
          ),
          listen(
            button,
            "click",
            /*click_handler_4*/
            ctx[228]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g;
      if (dirty[2] & /*fskBaudOptions*/
      128) {
        each_value_20 = ensure_array_like(
          /*fskBaudOptions*/
          ctx2[69]
        );
        let i;
        for (i = 0; i < each_value_20.length; i += 1) {
          const child_ctx = get_each_context_20(ctx2, each_value_20, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_20(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_20.length;
      }
      if (dirty[1] & /*activeFsk1*/
      512 | dirty[2] & /*fskBaudOptions*/
      128 && select0_value_value !== (select0_value_value = /*activeFsk1*/
      (_a = ctx2[40]) == null ? void 0 : _a.baud)) {
        select_option(
          select0,
          /*activeFsk1*/
          (_b = ctx2[40]) == null ? void 0 : _b.baud
        );
      }
      if (dirty[2] & /*fskDataBitsOptions*/
      256) {
        each_value_19 = ensure_array_like(
          /*fskDataBitsOptions*/
          ctx2[70]
        );
        let i;
        for (i = 0; i < each_value_19.length; i += 1) {
          const child_ctx = get_each_context_19(ctx2, each_value_19, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_19(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_19.length;
      }
      if (dirty[1] & /*activeFsk1*/
      512 | dirty[2] & /*fskBaudOptions*/
      128 && select1_value_value !== (select1_value_value = /*activeFsk1*/
      (_c = ctx2[40]) == null ? void 0 : _c.databits)) {
        select_option(
          select1,
          /*activeFsk1*/
          (_d = ctx2[40]) == null ? void 0 : _d.databits
        );
      }
      if (dirty[2] & /*fskStopBitsOptions*/
      512) {
        each_value_18 = ensure_array_like(
          /*fskStopBitsOptions*/
          ctx2[71]
        );
        let i;
        for (i = 0; i < each_value_18.length; i += 1) {
          const child_ctx = get_each_context_18(ctx2, each_value_18, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_18(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_18.length;
      }
      if (dirty[1] & /*activeFsk1*/
      512 | dirty[2] & /*fskBaudOptions*/
      128 && select2_value_value !== (select2_value_value = /*activeFsk1*/
      (_e = ctx2[40]) == null ? void 0 : _e.stopbits)) {
        select_option(
          select2,
          /*activeFsk1*/
          (_f = ctx2[40]) == null ? void 0 : _f.stopbits
        );
      }
      if (dirty[1] & /*activeFsk1*/
      512 | dirty[2] & /*fskBaudOptions*/
      128 && input0_checked_value !== (input0_checked_value = !!/*activeFsk1*/
      ((_g = ctx2[40]) == null ? void 0 : _g.rtscts))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1InvertFsk"
      ))) {
        input1.checked = input1_checked_value;
      }
      if (
        /*hasPfsk*/
        ctx2[62]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_28(ctx2);
          if_block0.c();
          if_block0.m(div16, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*radioStatus*/
        ctx2[8][`${/*activeSerial*/
        ctx2[0]}:fsk1`]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_27(ctx2);
          if_block1.c();
          if_block1.m(section, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_20(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_19(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_18(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_28(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Pseudo FSK:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "usePFsk"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_37*/
          ctx[227]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "usePFsk"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_27(ctx) {
  let div;
  let t_value = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:fsk1`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*radioStatus*/
      ctx[8][`${/*activeSerial*/
      ctx[0]}:fsk1`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 && t_value !== (t_value = /*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:fsk1`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:fsk1`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_23(ctx) {
  let section;
  let div0;
  let t1;
  let div16;
  let div3;
  let div1;
  let t3;
  let div2;
  let select0;
  let select0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let select1;
  let select1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let select2;
  let select2_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let input0;
  let input0_checked_value;
  let t13;
  let div15;
  let div13;
  let t15;
  let div14;
  let input1;
  let input1_checked_value;
  let t16;
  let t17;
  let div17;
  let button;
  let t19;
  let mounted;
  let dispose;
  let each_value_17 = ensure_array_like(
    /*fskBaudOptions*/
    ctx[69]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_17.length; i += 1) {
    each_blocks_2[i] = create_each_block_17(get_each_context_17(ctx, each_value_17, i));
  }
  let each_value_16 = ensure_array_like(
    /*fskDataBitsOptions*/
    ctx[70]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_16.length; i += 1) {
    each_blocks_1[i] = create_each_block_16(get_each_context_16(ctx, each_value_16, i));
  }
  let each_value_15 = ensure_array_like(
    /*fskStopBitsOptions*/
    ctx[71]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_15.length; i += 1) {
    each_blocks[i] = create_each_block_15(get_each_context_15(ctx, each_value_15, i));
  }
  let if_block0 = (
    /*hasPfsk*/
    ctx[62] && create_if_block_25(ctx)
  );
  let if_block1 = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:fsk2`] && create_if_block_24(ctx)
  );
  return {
    c() {
      var _a;
      section = element("section");
      div0 = element("div");
      div0.textContent = "FSK 2";
      t1 = space();
      div16 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Baud Rate:";
      t3 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Data Bits:";
      t6 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Stop Bits:";
      t9 = space();
      div8 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "RTS/CTS Handshake:";
      t12 = space();
      div11 = element("div");
      input0 = element("input");
      t13 = space();
      div15 = element("div");
      div13 = element("div");
      div13.textContent = "Invert FSK:";
      t15 = space();
      div14 = element("div");
      input1 = element("input");
      t16 = space();
      if (if_block0) if_block0.c();
      t17 = space();
      div17 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t19 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(select2, "class", "select");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*activeFsk2*/
      ((_a = ctx[39]) == null ? void 0 : _a.rtscts);
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2InvertFsk"
      );
      attr(div14, "class", "value");
      attr(div15, "class", "row");
      attr(div16, "class", "panel");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div17, "class", "button-row");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a, _b, _c;
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div16);
      append(div16, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeFsk2*/
        (_a = ctx[39]) == null ? void 0 : _a.baud
      );
      append(div16, t4);
      append(div16, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeFsk2*/
        (_b = ctx[39]) == null ? void 0 : _b.databits
      );
      append(div16, t7);
      append(div16, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*activeFsk2*/
        (_c = ctx[39]) == null ? void 0 : _c.stopbits
      );
      append(div16, t10);
      append(div16, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, input0);
      append(div16, t13);
      append(div16, div15);
      append(div15, div13);
      append(div15, t15);
      append(div15, div14);
      append(div14, input1);
      append(div16, t16);
      if (if_block0) if_block0.m(div16, null);
      append(section, t17);
      append(section, div17);
      append(div17, button);
      append(section, t19);
      if (if_block1) if_block1.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_38*/
            ctx[229]
          ),
          listen(
            select1,
            "change",
            /*change_handler_39*/
            ctx[230]
          ),
          listen(
            select2,
            "change",
            /*change_handler_40*/
            ctx[231]
          ),
          listen(
            input0,
            "change",
            /*change_handler_41*/
            ctx[232]
          ),
          listen(
            input1,
            "change",
            /*change_handler_42*/
            ctx[233]
          ),
          listen(
            button,
            "click",
            /*click_handler_5*/
            ctx[235]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g;
      if (dirty[2] & /*fskBaudOptions*/
      128) {
        each_value_17 = ensure_array_like(
          /*fskBaudOptions*/
          ctx2[69]
        );
        let i;
        for (i = 0; i < each_value_17.length; i += 1) {
          const child_ctx = get_each_context_17(ctx2, each_value_17, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_17(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_17.length;
      }
      if (dirty[1] & /*activeFsk2*/
      256 | dirty[2] & /*fskBaudOptions*/
      128 && select0_value_value !== (select0_value_value = /*activeFsk2*/
      (_a = ctx2[39]) == null ? void 0 : _a.baud)) {
        select_option(
          select0,
          /*activeFsk2*/
          (_b = ctx2[39]) == null ? void 0 : _b.baud
        );
      }
      if (dirty[2] & /*fskDataBitsOptions*/
      256) {
        each_value_16 = ensure_array_like(
          /*fskDataBitsOptions*/
          ctx2[70]
        );
        let i;
        for (i = 0; i < each_value_16.length; i += 1) {
          const child_ctx = get_each_context_16(ctx2, each_value_16, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_16(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_16.length;
      }
      if (dirty[1] & /*activeFsk2*/
      256 | dirty[2] & /*fskBaudOptions*/
      128 && select1_value_value !== (select1_value_value = /*activeFsk2*/
      (_c = ctx2[39]) == null ? void 0 : _c.databits)) {
        select_option(
          select1,
          /*activeFsk2*/
          (_d = ctx2[39]) == null ? void 0 : _d.databits
        );
      }
      if (dirty[2] & /*fskStopBitsOptions*/
      512) {
        each_value_15 = ensure_array_like(
          /*fskStopBitsOptions*/
          ctx2[71]
        );
        let i;
        for (i = 0; i < each_value_15.length; i += 1) {
          const child_ctx = get_each_context_15(ctx2, each_value_15, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_15(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_15.length;
      }
      if (dirty[1] & /*activeFsk2*/
      256 | dirty[2] & /*fskBaudOptions*/
      128 && select2_value_value !== (select2_value_value = /*activeFsk2*/
      (_e = ctx2[39]) == null ? void 0 : _e.stopbits)) {
        select_option(
          select2,
          /*activeFsk2*/
          (_f = ctx2[39]) == null ? void 0 : _f.stopbits
        );
      }
      if (dirty[1] & /*activeFsk2*/
      256 | dirty[2] & /*fskBaudOptions*/
      128 && input0_checked_value !== (input0_checked_value = !!/*activeFsk2*/
      ((_g = ctx2[39]) == null ? void 0 : _g.rtscts))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input1_checked_value !== (input1_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2InvertFsk"
      ))) {
        input1.checked = input1_checked_value;
      }
      if (
        /*hasPfsk*/
        ctx2[62]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_25(ctx2);
          if_block0.c();
          if_block0.m(div16, null);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*radioStatus*/
        ctx2[8][`${/*activeSerial*/
        ctx2[0]}:fsk2`]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_24(ctx2);
          if_block1.c();
          if_block1.m(section, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_17(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_16(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_15(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_25(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Pseudo FSK:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "usePFsk"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_43*/
          ctx[234]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "usePFsk"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_24(ctx) {
  let div;
  let t_value = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:fsk2`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*radioStatus*/
      ctx[8][`${/*activeSerial*/
      ctx[0]}:fsk2`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 && t_value !== (t_value = /*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:fsk2`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:fsk2`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_22(ctx) {
  let section;
  return {
    c() {
      section = element("section");
      section.innerHTML = `<div class="section-title">FSK</div> <div class="panel"><div class="placeholder">FSK is not supported on this device.</div></div>`;
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
    }
  };
}
function create_else_block_1(ctx) {
  let section;
  return {
    c() {
      section = element("section");
      section.innerHTML = `<div class="section-title">AUX</div> <div class="panel"><div class="placeholder">AUX is not supported on this device.</div></div>`;
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
      }
    }
  };
}
function create_if_block_19(ctx) {
  let section;
  let div0;
  let t1;
  let div13;
  let div3;
  let div1;
  let t3;
  let div2;
  let select0;
  let select0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let select1;
  let select1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let select2;
  let select2_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let input;
  let input_checked_value;
  let t13;
  let div14;
  let button;
  let t15;
  let mounted;
  let dispose;
  let each_value_14 = ensure_array_like(
    /*radioBaudOptions*/
    ctx[66]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_14.length; i += 1) {
    each_blocks_2[i] = create_each_block_14(get_each_context_14(ctx, each_value_14, i));
  }
  let each_value_13 = ensure_array_like(
    /*radioDataBitsOptions*/
    ctx[67]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_13.length; i += 1) {
    each_blocks_1[i] = create_each_block_13(get_each_context_13(ctx, each_value_13, i));
  }
  let each_value_12 = ensure_array_like(
    /*radioStopBitsOptions*/
    ctx[68]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_12.length; i += 1) {
    each_blocks[i] = create_each_block_12(get_each_context_12(ctx, each_value_12, i));
  }
  let if_block = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:aux`] && create_if_block_20(ctx)
  );
  return {
    c() {
      var _a;
      section = element("section");
      div0 = element("div");
      div0.textContent = "AUX";
      t1 = space();
      div13 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Baud Rate:";
      t3 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Data Bits:";
      t6 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Stop Bits:";
      t9 = space();
      div8 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "RTS/CTS Handshake:";
      t12 = space();
      div11 = element("div");
      input = element("input");
      t13 = space();
      div14 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t15 = space();
      if (if_block) if_block.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(select2, "class", "select");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*activeAux*/
      ((_a = ctx[41]) == null ? void 0 : _a.rtscts);
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "panel");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div14, "class", "button-row");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a, _b, _c;
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div13);
      append(div13, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeAux*/
        (_a = ctx[41]) == null ? void 0 : _a.baud
      );
      append(div13, t4);
      append(div13, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeAux*/
        (_b = ctx[41]) == null ? void 0 : _b.databits
      );
      append(div13, t7);
      append(div13, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*activeAux*/
        (_c = ctx[41]) == null ? void 0 : _c.stopbits
      );
      append(div13, t10);
      append(div13, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, input);
      append(section, t13);
      append(section, div14);
      append(div14, button);
      append(section, t15);
      if (if_block) if_block.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_28*/
            ctx[217]
          ),
          listen(
            select1,
            "change",
            /*change_handler_29*/
            ctx[218]
          ),
          listen(
            select2,
            "change",
            /*change_handler_30*/
            ctx[219]
          ),
          listen(
            input,
            "change",
            /*change_handler_31*/
            ctx[220]
          ),
          listen(
            button,
            "click",
            /*click_handler_3*/
            ctx[221]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g;
      if (dirty[2] & /*radioBaudOptions*/
      16) {
        each_value_14 = ensure_array_like(
          /*radioBaudOptions*/
          ctx2[66]
        );
        let i;
        for (i = 0; i < each_value_14.length; i += 1) {
          const child_ctx = get_each_context_14(ctx2, each_value_14, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_14(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_14.length;
      }
      if (dirty[1] & /*activeAux*/
      1024 | dirty[2] & /*radioBaudOptions*/
      16 && select0_value_value !== (select0_value_value = /*activeAux*/
      (_a = ctx2[41]) == null ? void 0 : _a.baud)) {
        select_option(
          select0,
          /*activeAux*/
          (_b = ctx2[41]) == null ? void 0 : _b.baud
        );
      }
      if (dirty[2] & /*radioDataBitsOptions*/
      32) {
        each_value_13 = ensure_array_like(
          /*radioDataBitsOptions*/
          ctx2[67]
        );
        let i;
        for (i = 0; i < each_value_13.length; i += 1) {
          const child_ctx = get_each_context_13(ctx2, each_value_13, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_13(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_13.length;
      }
      if (dirty[1] & /*activeAux*/
      1024 | dirty[2] & /*radioBaudOptions*/
      16 && select1_value_value !== (select1_value_value = /*activeAux*/
      (_c = ctx2[41]) == null ? void 0 : _c.databits)) {
        select_option(
          select1,
          /*activeAux*/
          (_d = ctx2[41]) == null ? void 0 : _d.databits
        );
      }
      if (dirty[2] & /*radioStopBitsOptions*/
      64) {
        each_value_12 = ensure_array_like(
          /*radioStopBitsOptions*/
          ctx2[68]
        );
        let i;
        for (i = 0; i < each_value_12.length; i += 1) {
          const child_ctx = get_each_context_12(ctx2, each_value_12, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_12(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_12.length;
      }
      if (dirty[1] & /*activeAux*/
      1024 | dirty[2] & /*radioBaudOptions*/
      16 && select2_value_value !== (select2_value_value = /*activeAux*/
      (_e = ctx2[41]) == null ? void 0 : _e.stopbits)) {
        select_option(
          select2,
          /*activeAux*/
          (_f = ctx2[41]) == null ? void 0 : _f.stopbits
        );
      }
      if (dirty[1] & /*activeAux*/
      1024 | dirty[2] & /*radioBaudOptions*/
      16 && input_checked_value !== (input_checked_value = !!/*activeAux*/
      ((_g = ctx2[41]) == null ? void 0 : _g.rtscts))) {
        input.checked = input_checked_value;
      }
      if (
        /*radioStatus*/
        ctx2[8][`${/*activeSerial*/
        ctx2[0]}:aux`]
      ) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block_20(ctx2);
          if_block.c();
          if_block.m(section, null);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block) if_block.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_14(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_13(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_12(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_20(ctx) {
  let div;
  let t_value = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:aux`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*radioStatus*/
      ctx[8][`${/*activeSerial*/
      ctx[0]}:aux`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 && t_value !== (t_value = /*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:aux`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:aux`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_15(ctx) {
  let section;
  let div0;
  let t1;
  let div13;
  let div3;
  let div1;
  let t3;
  let div2;
  let select0;
  let select0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let select1;
  let select1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let select2;
  let select2_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let input;
  let input_checked_value;
  let t13;
  let show_if = (
    /*hasFlag*/
    ctx[86](
      /*activeSerial*/
      ctx[0],
      "HAS_R1_RADIO_SUPPORT"
    )
  );
  let t14;
  let div14;
  let button;
  let t16;
  let mounted;
  let dispose;
  let each_value_11 = ensure_array_like(
    /*radioBaudOptions*/
    ctx[66]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_11.length; i += 1) {
    each_blocks_2[i] = create_each_block_11(get_each_context_11(ctx, each_value_11, i));
  }
  let each_value_10 = ensure_array_like(
    /*radioDataBitsOptions*/
    ctx[67]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_10.length; i += 1) {
    each_blocks_1[i] = create_each_block_10(get_each_context_10(ctx, each_value_10, i));
  }
  let each_value_9 = ensure_array_like(
    /*radioStopBitsOptions*/
    ctx[68]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_9.length; i += 1) {
    each_blocks[i] = create_each_block_9(get_each_context_9(ctx, each_value_9, i));
  }
  let if_block0 = show_if && create_if_block_17(ctx);
  let if_block1 = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:r1`] && create_if_block_16(ctx)
  );
  return {
    c() {
      var _a;
      section = element("section");
      div0 = element("div");
      div0.textContent = "Radio 1";
      t1 = space();
      div13 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Baud Rate:";
      t3 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Data Bits:";
      t6 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Stop Bits:";
      t9 = space();
      div8 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "RTS/CTS Handshake:";
      t12 = space();
      div11 = element("div");
      input = element("input");
      t13 = space();
      if (if_block0) if_block0.c();
      t14 = space();
      div14 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t16 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(select2, "class", "select");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*activeR1*/
      ((_a = ctx[43]) == null ? void 0 : _a.rtscts);
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "panel");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div14, "class", "button-row");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a, _b, _c;
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div13);
      append(div13, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeR1*/
        (_a = ctx[43]) == null ? void 0 : _a.baud
      );
      append(div13, t4);
      append(div13, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeR1*/
        (_b = ctx[43]) == null ? void 0 : _b.databits
      );
      append(div13, t7);
      append(div13, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*activeR1*/
        (_c = ctx[43]) == null ? void 0 : _c.stopbits
      );
      append(div13, t10);
      append(div13, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, input);
      append(section, t13);
      if (if_block0) if_block0.m(section, null);
      append(section, t14);
      append(section, div14);
      append(div14, button);
      append(section, t16);
      if (if_block1) if_block1.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_4*/
            ctx[176]
          ),
          listen(
            select1,
            "change",
            /*change_handler_5*/
            ctx[177]
          ),
          listen(
            select2,
            "change",
            /*change_handler_6*/
            ctx[178]
          ),
          listen(
            input,
            "change",
            /*change_handler_7*/
            ctx[179]
          ),
          listen(
            button,
            "click",
            /*click_handler*/
            ctx[186]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g;
      if (dirty[2] & /*radioBaudOptions*/
      16) {
        each_value_11 = ensure_array_like(
          /*radioBaudOptions*/
          ctx2[66]
        );
        let i;
        for (i = 0; i < each_value_11.length; i += 1) {
          const child_ctx = get_each_context_11(ctx2, each_value_11, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_11(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_11.length;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && select0_value_value !== (select0_value_value = /*activeR1*/
      (_a = ctx2[43]) == null ? void 0 : _a.baud)) {
        select_option(
          select0,
          /*activeR1*/
          (_b = ctx2[43]) == null ? void 0 : _b.baud
        );
      }
      if (dirty[2] & /*radioDataBitsOptions*/
      32) {
        each_value_10 = ensure_array_like(
          /*radioDataBitsOptions*/
          ctx2[67]
        );
        let i;
        for (i = 0; i < each_value_10.length; i += 1) {
          const child_ctx = get_each_context_10(ctx2, each_value_10, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_10(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_10.length;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && select1_value_value !== (select1_value_value = /*activeR1*/
      (_c = ctx2[43]) == null ? void 0 : _c.databits)) {
        select_option(
          select1,
          /*activeR1*/
          (_d = ctx2[43]) == null ? void 0 : _d.databits
        );
      }
      if (dirty[2] & /*radioStopBitsOptions*/
      64) {
        each_value_9 = ensure_array_like(
          /*radioStopBitsOptions*/
          ctx2[68]
        );
        let i;
        for (i = 0; i < each_value_9.length; i += 1) {
          const child_ctx = get_each_context_9(ctx2, each_value_9, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_9(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_9.length;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && select2_value_value !== (select2_value_value = /*activeR1*/
      (_e = ctx2[43]) == null ? void 0 : _e.stopbits)) {
        select_option(
          select2,
          /*activeR1*/
          (_f = ctx2[43]) == null ? void 0 : _f.stopbits
        );
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && input_checked_value !== (input_checked_value = !!/*activeR1*/
      ((_g = ctx2[43]) == null ? void 0 : _g.rtscts))) {
        input.checked = input_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1) show_if = /*hasFlag*/
      ctx2[86](
        /*activeSerial*/
        ctx2[0],
        "HAS_R1_RADIO_SUPPORT"
      );
      if (show_if) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_17(ctx2);
          if_block0.c();
          if_block0.m(section, t14);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*radioStatus*/
        ctx2[8][`${/*activeSerial*/
        ctx2[0]}:r1`]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_16(ctx2);
          if_block1.c();
          if_block1.m(section, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_11(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_10(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_9(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_17(ctx) {
  let div18;
  let div2;
  let div0;
  let t1;
  let div1;
  let select0;
  let select0_value_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input0;
  let input0_value_value;
  let t5;
  let div8;
  let div6;
  let t7;
  let div7;
  let input1;
  let input1_checked_value;
  let t8;
  let div11;
  let div9;
  let t10;
  let div10;
  let select1;
  let select1_value_value;
  let t11;
  let div14;
  let div12;
  let t13;
  let div13;
  let input2;
  let input2_checked_value;
  let t14;
  let div17;
  let div15;
  let t16;
  let div16;
  let input3;
  let input3_checked_value;
  let mounted;
  let dispose;
  let each_value_8 = ensure_array_like(
    /*rigtypes*/
    ctx[103]()
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_8.length; i += 1) {
    each_blocks_1[i] = create_each_block_8(get_each_context_8(ctx, each_value_8, i));
  }
  let each_value_7 = ensure_array_like(
    /*digitalOverVoiceOptions*/
    ctx[80]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_7.length; i += 1) {
    each_blocks[i] = create_each_block_7(get_each_context_7(ctx, each_value_7, i));
  }
  return {
    c() {
      var _a, _b, _c, _d;
      div18 = element("div");
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Rig Type:";
      t1 = space();
      div1 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "Icom Address:";
      t4 = space();
      div4 = element("div");
      input0 = element("input");
      t5 = space();
      div8 = element("div");
      div6 = element("div");
      div6.textContent = "PW1 Connected:";
      t7 = space();
      div7 = element("div");
      input1 = element("input");
      t8 = space();
      div11 = element("div");
      div9 = element("div");
      div9.textContent = "Digital Over Voice:";
      t10 = space();
      div10 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t11 = space();
      div14 = element("div");
      div12 = element("div");
      div12.textContent = "Use Decoder if Connected:";
      t13 = space();
      div13 = element("div");
      input2 = element("input");
      t14 = space();
      div17 = element("div");
      div15 = element("div");
      div15.textContent = "Don't Interfere USB control:";
      t16 = space();
      div16 = element("div");
      input3 = element("input");
      attr(div0, "class", "label");
      attr(select0, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*activeR1*/
      (_a = ctx[43]) == null ? void 0 : _a.icomaddress;
      attr(div4, "class", "value");
      attr(div5, "class", "row");
      attr(div6, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*activeR1*/
      ((_b = ctx[43]) == null ? void 0 : _b.icomsimulateautoinfo);
      attr(div7, "class", "value");
      attr(div8, "class", "row");
      attr(div9, "class", "label");
      attr(select1, "class", "select");
      attr(div10, "class", "value");
      attr(div11, "class", "row");
      attr(div12, "class", "label");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*activeR1*/
      ((_c = ctx[43]) == null ? void 0 : _c.usedecoderifconnected);
      attr(div13, "class", "value");
      attr(div14, "class", "row");
      attr(div15, "class", "label");
      attr(input3, "type", "checkbox");
      input3.checked = input3_checked_value = !!/*activeR1*/
      ((_d = ctx[43]) == null ? void 0 : _d.dontinterfereusbcontrol);
      attr(div16, "class", "value");
      attr(div17, "class", "row");
      attr(div18, "class", "panel");
      set_style(div18, "margin-top", "12px");
    },
    m(target, anchor) {
      var _a, _b;
      insert(target, div18, anchor);
      append(div18, div2);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, select0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeR1*/
        (_a = ctx[43]) == null ? void 0 : _a.rigtype
      );
      append(div18, t2);
      append(div18, div5);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input0);
      append(div18, t5);
      append(div18, div8);
      append(div8, div6);
      append(div8, t7);
      append(div8, div7);
      append(div7, input1);
      append(div18, t8);
      append(div18, div11);
      append(div11, div9);
      append(div11, t10);
      append(div11, div10);
      append(div10, select1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeR1*/
        (_b = ctx[43]) == null ? void 0 : _b.digitalovervoicerule
      );
      append(div18, t11);
      append(div18, div14);
      append(div14, div12);
      append(div14, t13);
      append(div14, div13);
      append(div13, input2);
      append(div18, t14);
      append(div18, div17);
      append(div17, div15);
      append(div17, t16);
      append(div17, div16);
      append(div16, input3);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_8*/
            ctx[180]
          ),
          listen(
            input0,
            "input",
            /*input_handler*/
            ctx[181]
          ),
          listen(
            input1,
            "change",
            /*change_handler_9*/
            ctx[182]
          ),
          listen(
            select1,
            "change",
            /*change_handler_10*/
            ctx[183]
          ),
          listen(
            input2,
            "change",
            /*change_handler_11*/
            ctx[184]
          ),
          listen(
            input3,
            "change",
            /*change_handler_12*/
            ctx[185]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g, _h;
      if (dirty[3] & /*rigtypes*/
      1024) {
        each_value_8 = ensure_array_like(
          /*rigtypes*/
          ctx2[103]()
        );
        let i;
        for (i = 0; i < each_value_8.length; i += 1) {
          const child_ctx = get_each_context_8(ctx2, each_value_8, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_8(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_8.length;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && select0_value_value !== (select0_value_value = /*activeR1*/
      (_a = ctx2[43]) == null ? void 0 : _a.rigtype)) {
        select_option(
          select0,
          /*activeR1*/
          (_b = ctx2[43]) == null ? void 0 : _b.rigtype
        );
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && input0_value_value !== (input0_value_value = /*activeR1*/
      (_c = ctx2[43]) == null ? void 0 : _c.icomaddress) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && input1_checked_value !== (input1_checked_value = !!/*activeR1*/
      ((_d = ctx2[43]) == null ? void 0 : _d.icomsimulateautoinfo))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[2] & /*digitalOverVoiceOptions*/
      262144) {
        each_value_7 = ensure_array_like(
          /*digitalOverVoiceOptions*/
          ctx2[80]
        );
        let i;
        for (i = 0; i < each_value_7.length; i += 1) {
          const child_ctx = get_each_context_7(ctx2, each_value_7, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_7(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_7.length;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && select1_value_value !== (select1_value_value = /*activeR1*/
      (_e = ctx2[43]) == null ? void 0 : _e.digitalovervoicerule)) {
        select_option(
          select1,
          /*activeR1*/
          (_f = ctx2[43]) == null ? void 0 : _f.digitalovervoicerule
        );
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && input2_checked_value !== (input2_checked_value = !!/*activeR1*/
      ((_g = ctx2[43]) == null ? void 0 : _g.usedecoderifconnected))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[1] & /*activeR1*/
      4096 | dirty[2] & /*radioBaudOptions*/
      16 && input3_checked_value !== (input3_checked_value = !!/*activeR1*/
      ((_h = ctx2[43]) == null ? void 0 : _h.dontinterfereusbcontrol))) {
        input3.checked = input3_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div18);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_8(ctx) {
  let option;
  let t_value = (
    /*rig*/
    ctx[496].name + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*rig*/
      ctx[496].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_7(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_16(ctx) {
  let div;
  let t_value = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:r1`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*radioStatus*/
      ctx[8][`${/*activeSerial*/
      ctx[0]}:r1`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 && t_value !== (t_value = /*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:r1`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:r1`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_12(ctx) {
  let section;
  let div0;
  let t1;
  let div13;
  let div3;
  let div1;
  let t3;
  let div2;
  let select0;
  let select0_value_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let select1;
  let select1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let select2;
  let select2_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let input;
  let input_checked_value;
  let t13;
  let show_if = (
    /*hasFlag*/
    ctx[86](
      /*activeSerial*/
      ctx[0],
      "HAS_R2_RADIO_SUPPORT"
    )
  );
  let t14;
  let div14;
  let button;
  let t16;
  let mounted;
  let dispose;
  let each_value_6 = ensure_array_like(
    /*radioBaudOptions*/
    ctx[66]
  );
  let each_blocks_2 = [];
  for (let i = 0; i < each_value_6.length; i += 1) {
    each_blocks_2[i] = create_each_block_6(get_each_context_6(ctx, each_value_6, i));
  }
  let each_value_5 = ensure_array_like(
    /*radioDataBitsOptions*/
    ctx[67]
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_5.length; i += 1) {
    each_blocks_1[i] = create_each_block_5(get_each_context_5(ctx, each_value_5, i));
  }
  let each_value_4 = ensure_array_like(
    /*radioStopBitsOptions*/
    ctx[68]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_4.length; i += 1) {
    each_blocks[i] = create_each_block_4(get_each_context_4(ctx, each_value_4, i));
  }
  let if_block0 = show_if && create_if_block_14(ctx);
  let if_block1 = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:r2`] && create_if_block_13(ctx)
  );
  return {
    c() {
      var _a;
      section = element("section");
      div0 = element("div");
      div0.textContent = "Radio 2";
      t1 = space();
      div13 = element("div");
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Baud Rate:";
      t3 = space();
      div2 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        each_blocks_2[i].c();
      }
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Data Bits:";
      t6 = space();
      div5 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Stop Bits:";
      t9 = space();
      div8 = element("div");
      select2 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "RTS/CTS Handshake:";
      t12 = space();
      div11 = element("div");
      input = element("input");
      t13 = space();
      if (if_block0) if_block0.c();
      t14 = space();
      div14 = element("div");
      button = element("button");
      button.textContent = "Apply";
      t16 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select0, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(select1, "class", "select");
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(select2, "class", "select");
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*activeR2*/
      ((_a = ctx[42]) == null ? void 0 : _a.rtscts);
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "panel");
      attr(button, "class", "btn");
      attr(button, "type", "button");
      attr(div14, "class", "button-row");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      var _a, _b, _c;
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div13);
      append(div13, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, select0);
      for (let i = 0; i < each_blocks_2.length; i += 1) {
        if (each_blocks_2[i]) {
          each_blocks_2[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeR2*/
        (_a = ctx[42]) == null ? void 0 : _a.baud
      );
      append(div13, t4);
      append(div13, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, select1);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeR2*/
        (_b = ctx[42]) == null ? void 0 : _b.databits
      );
      append(div13, t7);
      append(div13, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, select2);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select2, null);
        }
      }
      select_option(
        select2,
        /*activeR2*/
        (_c = ctx[42]) == null ? void 0 : _c.stopbits
      );
      append(div13, t10);
      append(div13, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, input);
      append(section, t13);
      if (if_block0) if_block0.m(section, null);
      append(section, t14);
      append(section, div14);
      append(div14, button);
      append(section, t16);
      if (if_block1) if_block1.m(section, null);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_13*/
            ctx[187]
          ),
          listen(
            select1,
            "change",
            /*change_handler_14*/
            ctx[188]
          ),
          listen(
            select2,
            "change",
            /*change_handler_15*/
            ctx[189]
          ),
          listen(
            input,
            "change",
            /*change_handler_16*/
            ctx[190]
          ),
          listen(
            button,
            "click",
            /*click_handler_1*/
            ctx[197]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g;
      if (dirty[2] & /*radioBaudOptions*/
      16) {
        each_value_6 = ensure_array_like(
          /*radioBaudOptions*/
          ctx2[66]
        );
        let i;
        for (i = 0; i < each_value_6.length; i += 1) {
          const child_ctx = get_each_context_6(ctx2, each_value_6, i);
          if (each_blocks_2[i]) {
            each_blocks_2[i].p(child_ctx, dirty);
          } else {
            each_blocks_2[i] = create_each_block_6(child_ctx);
            each_blocks_2[i].c();
            each_blocks_2[i].m(select0, null);
          }
        }
        for (; i < each_blocks_2.length; i += 1) {
          each_blocks_2[i].d(1);
        }
        each_blocks_2.length = each_value_6.length;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && select0_value_value !== (select0_value_value = /*activeR2*/
      (_a = ctx2[42]) == null ? void 0 : _a.baud)) {
        select_option(
          select0,
          /*activeR2*/
          (_b = ctx2[42]) == null ? void 0 : _b.baud
        );
      }
      if (dirty[2] & /*radioDataBitsOptions*/
      32) {
        each_value_5 = ensure_array_like(
          /*radioDataBitsOptions*/
          ctx2[67]
        );
        let i;
        for (i = 0; i < each_value_5.length; i += 1) {
          const child_ctx = get_each_context_5(ctx2, each_value_5, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_5(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select1, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_5.length;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && select1_value_value !== (select1_value_value = /*activeR2*/
      (_c = ctx2[42]) == null ? void 0 : _c.databits)) {
        select_option(
          select1,
          /*activeR2*/
          (_d = ctx2[42]) == null ? void 0 : _d.databits
        );
      }
      if (dirty[2] & /*radioStopBitsOptions*/
      64) {
        each_value_4 = ensure_array_like(
          /*radioStopBitsOptions*/
          ctx2[68]
        );
        let i;
        for (i = 0; i < each_value_4.length; i += 1) {
          const child_ctx = get_each_context_4(ctx2, each_value_4, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_4(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select2, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_4.length;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && select2_value_value !== (select2_value_value = /*activeR2*/
      (_e = ctx2[42]) == null ? void 0 : _e.stopbits)) {
        select_option(
          select2,
          /*activeR2*/
          (_f = ctx2[42]) == null ? void 0 : _f.stopbits
        );
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && input_checked_value !== (input_checked_value = !!/*activeR2*/
      ((_g = ctx2[42]) == null ? void 0 : _g.rtscts))) {
        input.checked = input_checked_value;
      }
      if (dirty[0] & /*activeSerial*/
      1) show_if = /*hasFlag*/
      ctx2[86](
        /*activeSerial*/
        ctx2[0],
        "HAS_R2_RADIO_SUPPORT"
      );
      if (show_if) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_14(ctx2);
          if_block0.c();
          if_block0.m(section, t14);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (
        /*radioStatus*/
        ctx2[8][`${/*activeSerial*/
        ctx2[0]}:r2`]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_13(ctx2);
          if_block1.c();
          if_block1.m(section, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      destroy_each(each_blocks_2, detaching);
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      if (if_block0) if_block0.d();
      if (if_block1) if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_6(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_5(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_4(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489] + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489];
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_14(ctx) {
  let div18;
  let div2;
  let div0;
  let t1;
  let div1;
  let select0;
  let select0_value_value;
  let t2;
  let div5;
  let div3;
  let t4;
  let div4;
  let input0;
  let input0_value_value;
  let t5;
  let div8;
  let div6;
  let t7;
  let div7;
  let input1;
  let input1_checked_value;
  let t8;
  let div11;
  let div9;
  let t10;
  let div10;
  let select1;
  let select1_value_value;
  let t11;
  let div14;
  let div12;
  let t13;
  let div13;
  let input2;
  let input2_checked_value;
  let t14;
  let div17;
  let div15;
  let t16;
  let div16;
  let input3;
  let input3_checked_value;
  let mounted;
  let dispose;
  let each_value_3 = ensure_array_like(
    /*rigtypes*/
    ctx[103]()
  );
  let each_blocks_1 = [];
  for (let i = 0; i < each_value_3.length; i += 1) {
    each_blocks_1[i] = create_each_block_3(get_each_context_3(ctx, each_value_3, i));
  }
  let each_value_2 = ensure_array_like(
    /*digitalOverVoiceOptions*/
    ctx[80]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_2.length; i += 1) {
    each_blocks[i] = create_each_block_2(get_each_context_2(ctx, each_value_2, i));
  }
  return {
    c() {
      var _a, _b, _c, _d;
      div18 = element("div");
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Rig Type:";
      t1 = space();
      div1 = element("div");
      select0 = element("select");
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        each_blocks_1[i].c();
      }
      t2 = space();
      div5 = element("div");
      div3 = element("div");
      div3.textContent = "Icom Address:";
      t4 = space();
      div4 = element("div");
      input0 = element("input");
      t5 = space();
      div8 = element("div");
      div6 = element("div");
      div6.textContent = "PW1 Connected:";
      t7 = space();
      div7 = element("div");
      input1 = element("input");
      t8 = space();
      div11 = element("div");
      div9 = element("div");
      div9.textContent = "Digital Over Voice:";
      t10 = space();
      div10 = element("div");
      select1 = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t11 = space();
      div14 = element("div");
      div12 = element("div");
      div12.textContent = "Use Decoder if Connected:";
      t13 = space();
      div13 = element("div");
      input2 = element("input");
      t14 = space();
      div17 = element("div");
      div15 = element("div");
      div15.textContent = "Don't Interfere USB control:";
      t16 = space();
      div16 = element("div");
      input3 = element("input");
      attr(div0, "class", "label");
      attr(select0, "class", "select");
      attr(div1, "class", "value");
      attr(div2, "class", "row");
      attr(div3, "class", "label");
      attr(input0, "class", "input");
      attr(input0, "type", "number");
      input0.value = input0_value_value = /*activeR2*/
      (_a = ctx[42]) == null ? void 0 : _a.icomaddress;
      attr(div4, "class", "value");
      attr(div5, "class", "row");
      attr(div6, "class", "label");
      attr(input1, "type", "checkbox");
      input1.checked = input1_checked_value = !!/*activeR2*/
      ((_b = ctx[42]) == null ? void 0 : _b.icomsimulateautoinfo);
      attr(div7, "class", "value");
      attr(div8, "class", "row");
      attr(div9, "class", "label");
      attr(select1, "class", "select");
      attr(div10, "class", "value");
      attr(div11, "class", "row");
      attr(div12, "class", "label");
      attr(input2, "type", "checkbox");
      input2.checked = input2_checked_value = !!/*activeR2*/
      ((_c = ctx[42]) == null ? void 0 : _c.usedecoderifconnected);
      attr(div13, "class", "value");
      attr(div14, "class", "row");
      attr(div15, "class", "label");
      attr(input3, "type", "checkbox");
      input3.checked = input3_checked_value = !!/*activeR2*/
      ((_d = ctx[42]) == null ? void 0 : _d.dontinterfereusbcontrol);
      attr(div16, "class", "value");
      attr(div17, "class", "row");
      attr(div18, "class", "panel");
      set_style(div18, "margin-top", "12px");
    },
    m(target, anchor) {
      var _a, _b;
      insert(target, div18, anchor);
      append(div18, div2);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, select0);
      for (let i = 0; i < each_blocks_1.length; i += 1) {
        if (each_blocks_1[i]) {
          each_blocks_1[i].m(select0, null);
        }
      }
      select_option(
        select0,
        /*activeR2*/
        (_a = ctx[42]) == null ? void 0 : _a.rigtype
      );
      append(div18, t2);
      append(div18, div5);
      append(div5, div3);
      append(div5, t4);
      append(div5, div4);
      append(div4, input0);
      append(div18, t5);
      append(div18, div8);
      append(div8, div6);
      append(div8, t7);
      append(div8, div7);
      append(div7, input1);
      append(div18, t8);
      append(div18, div11);
      append(div11, div9);
      append(div11, t10);
      append(div11, div10);
      append(div10, select1);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select1, null);
        }
      }
      select_option(
        select1,
        /*activeR2*/
        (_b = ctx[42]) == null ? void 0 : _b.digitalovervoicerule
      );
      append(div18, t11);
      append(div18, div14);
      append(div14, div12);
      append(div14, t13);
      append(div14, div13);
      append(div13, input2);
      append(div18, t14);
      append(div18, div17);
      append(div17, div15);
      append(div17, t16);
      append(div17, div16);
      append(div16, input3);
      if (!mounted) {
        dispose = [
          listen(
            select0,
            "change",
            /*change_handler_17*/
            ctx[191]
          ),
          listen(
            input0,
            "input",
            /*input_handler_1*/
            ctx[192]
          ),
          listen(
            input1,
            "change",
            /*change_handler_18*/
            ctx[193]
          ),
          listen(
            select1,
            "change",
            /*change_handler_19*/
            ctx[194]
          ),
          listen(
            input2,
            "change",
            /*change_handler_20*/
            ctx[195]
          ),
          listen(
            input3,
            "change",
            /*change_handler_21*/
            ctx[196]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b, _c, _d, _e, _f, _g, _h;
      if (dirty[3] & /*rigtypes*/
      1024) {
        each_value_3 = ensure_array_like(
          /*rigtypes*/
          ctx2[103]()
        );
        let i;
        for (i = 0; i < each_value_3.length; i += 1) {
          const child_ctx = get_each_context_3(ctx2, each_value_3, i);
          if (each_blocks_1[i]) {
            each_blocks_1[i].p(child_ctx, dirty);
          } else {
            each_blocks_1[i] = create_each_block_3(child_ctx);
            each_blocks_1[i].c();
            each_blocks_1[i].m(select0, null);
          }
        }
        for (; i < each_blocks_1.length; i += 1) {
          each_blocks_1[i].d(1);
        }
        each_blocks_1.length = each_value_3.length;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && select0_value_value !== (select0_value_value = /*activeR2*/
      (_a = ctx2[42]) == null ? void 0 : _a.rigtype)) {
        select_option(
          select0,
          /*activeR2*/
          (_b = ctx2[42]) == null ? void 0 : _b.rigtype
        );
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && input0_value_value !== (input0_value_value = /*activeR2*/
      (_c = ctx2[42]) == null ? void 0 : _c.icomaddress) && input0.value !== input0_value_value) {
        input0.value = input0_value_value;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && input1_checked_value !== (input1_checked_value = !!/*activeR2*/
      ((_d = ctx2[42]) == null ? void 0 : _d.icomsimulateautoinfo))) {
        input1.checked = input1_checked_value;
      }
      if (dirty[2] & /*digitalOverVoiceOptions*/
      262144) {
        each_value_2 = ensure_array_like(
          /*digitalOverVoiceOptions*/
          ctx2[80]
        );
        let i;
        for (i = 0; i < each_value_2.length; i += 1) {
          const child_ctx = get_each_context_2(ctx2, each_value_2, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_2(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select1, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_2.length;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && select1_value_value !== (select1_value_value = /*activeR2*/
      (_e = ctx2[42]) == null ? void 0 : _e.digitalovervoicerule)) {
        select_option(
          select1,
          /*activeR2*/
          (_f = ctx2[42]) == null ? void 0 : _f.digitalovervoicerule
        );
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && input2_checked_value !== (input2_checked_value = !!/*activeR2*/
      ((_g = ctx2[42]) == null ? void 0 : _g.usedecoderifconnected))) {
        input2.checked = input2_checked_value;
      }
      if (dirty[1] & /*activeR2*/
      2048 | dirty[2] & /*radioBaudOptions*/
      16 && input3_checked_value !== (input3_checked_value = !!/*activeR2*/
      ((_h = ctx2[42]) == null ? void 0 : _h.dontinterfereusbcontrol))) {
        input3.checked = input3_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div18);
      }
      destroy_each(each_blocks_1, detaching);
      destroy_each(each_blocks, detaching);
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_each_block_3(ctx) {
  let option;
  let t_value = (
    /*rig*/
    ctx[496].name + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*rig*/
      ctx[496].id;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_each_block_2(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_13(ctx) {
  let div;
  let t_value = (
    /*radioStatus*/
    ctx[8][`${/*activeSerial*/
    ctx[0]}:r2`].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*radioStatus*/
      ctx[8][`${/*activeSerial*/
      ctx[0]}:r2`].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 && t_value !== (t_value = /*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:r2`].text + "")) set_data(t, t_value);
      if (dirty[0] & /*radioStatus, activeSerial*/
      257 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*radioStatus*/
      ctx2[8][`${/*activeSerial*/
      ctx2[0]}:r2`].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_11(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_value_value;
  let mounted;
  let dispose;
  return {
    c() {
      var _a, _b;
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "rigctld options:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "class", "input");
      attr(input, "type", "text");
      attr(input, "placeholder", "-m 361 -r /dev/ttyUSB0");
      input.value = input_value_value = /*activeRigModeSync*/
      ((_b = (_a = ctx[38]) == null ? void 0 : _a.r1) == null ? void 0 : _b.rigctld_options) ?? "";
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "input",
          /*input_handler_7*/
          ctx[207]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b;
      if (dirty[1] & /*activeRigModeSync*/
      128 && input_value_value !== (input_value_value = /*activeRigModeSync*/
      ((_b = (_a = ctx2[38]) == null ? void 0 : _a.r1) == null ? void 0 : _b.rigctld_options) ?? "") && input.value !== input_value_value) {
        input.value = input_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_9(ctx) {
  var _a, _b;
  let div22;
  let div0;
  let t1;
  let div3;
  let div1;
  let t3;
  let div2;
  let input0;
  let input0_checked_value;
  let t4;
  let div6;
  let div4;
  let t6;
  let div5;
  let input1;
  let input1_value_value;
  let t7;
  let div9;
  let div7;
  let t9;
  let div8;
  let input2;
  let input2_value_value;
  let t10;
  let div12;
  let div10;
  let t12;
  let div11;
  let input3;
  let input3_value_value;
  let t13;
  let div15;
  let div13;
  let t15;
  let div14;
  let input4;
  let input4_value_value;
  let t16;
  let div18;
  let div16;
  let t18;
  let div17;
  let input5;
  let input5_value_value;
  let t19;
  let div21;
  let div19;
  let t21;
  let div20;
  let input6;
  let input6_checked_value;
  let t22;
  let mounted;
  let dispose;
  let if_block = (
    /*activeRigModeSync*/
    ((_b = (_a = ctx[38]) == null ? void 0 : _a.r2) == null ? void 0 : _b.auto_start) && create_if_block_10(ctx)
  );
  return {
    c() {
      var _a2, _b2, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n;
      div22 = element("div");
      div0 = element("div");
      div0.textContent = "Radio 2 Endpoint";
      t1 = space();
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Enabled:";
      t3 = space();
      div2 = element("div");
      input0 = element("input");
      t4 = space();
      div6 = element("div");
      div4 = element("div");
      div4.textContent = "Host:";
      t6 = space();
      div5 = element("div");
      input1 = element("input");
      t7 = space();
      div9 = element("div");
      div7 = element("div");
      div7.textContent = "Port:";
      t9 = space();
      div8 = element("div");
      input2 = element("input");
      t10 = space();
      div12 = element("div");
      div10 = element("div");
      div10.textContent = "Connect Timeout (ms):";
      t12 = space();
      div11 = element("div");
      input3 = element("input");
      t13 = space();
      div15 = element("div");
      div13 = element("div");
      div13.textContent = "I/O Timeout (ms):";
      t15 = space();
      div14 = element("div");
      input4 = element("input");
      t16 = space();
      div18 = element("div");
      div16 = element("div");
      div16.textContent = "Poll Interval (ms):";
      t18 = space();
      div17 = element("div");
      input5 = element("input");
      t19 = space();
      div21 = element("div");
      div19 = element("div");
      div19.textContent = "Auto-start rigctld:";
      t21 = space();
      div20 = element("div");
      input6 = element("input");
      t22 = space();
      if (if_block) if_block.c();
      attr(div0, "class", "section-subtitle");
      attr(div1, "class", "label");
      attr(input0, "type", "checkbox");
      input0.checked = input0_checked_value = !!/*activeRigModeSync*/
      ((_b2 = (_a2 = ctx[38]) == null ? void 0 : _a2.r2) == null ? void 0 : _b2.enabled);
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "label");
      attr(input1, "class", "input");
      attr(input1, "type", "text");
      input1.value = input1_value_value = /*activeRigModeSync*/
      ((_d = (_c = ctx[38]) == null ? void 0 : _c.r2) == null ? void 0 : _d.host) ?? "127.0.0.1";
      attr(div5, "class", "value");
      attr(div6, "class", "row");
      attr(div7, "class", "label");
      attr(input2, "class", "input");
      attr(input2, "type", "number");
      attr(input2, "min", "1");
      input2.value = input2_value_value = /*activeRigModeSync*/
      ((_f = (_e = ctx[38]) == null ? void 0 : _e.r2) == null ? void 0 : _f.port) ?? 4533;
      attr(div8, "class", "value");
      attr(div9, "class", "row");
      attr(div10, "class", "label");
      attr(input3, "class", "input");
      attr(input3, "type", "number");
      attr(input3, "min", "100");
      input3.value = input3_value_value = /*activeRigModeSync*/
      ((_h = (_g = ctx[38]) == null ? void 0 : _g.r2) == null ? void 0 : _h.connect_timeout_ms) ?? 1500;
      attr(div11, "class", "value");
      attr(div12, "class", "row");
      attr(div13, "class", "label");
      attr(input4, "class", "input");
      attr(input4, "type", "number");
      attr(input4, "min", "100");
      input4.value = input4_value_value = /*activeRigModeSync*/
      ((_j = (_i = ctx[38]) == null ? void 0 : _i.r2) == null ? void 0 : _j.io_timeout_ms) ?? 1500;
      attr(div14, "class", "value");
      attr(div15, "class", "row");
      attr(div16, "class", "label");
      attr(input5, "class", "input");
      attr(input5, "type", "number");
      attr(input5, "min", "100");
      input5.value = input5_value_value = /*activeRigModeSync*/
      ((_l = (_k = ctx[38]) == null ? void 0 : _k.r2) == null ? void 0 : _l.poll_ms) ?? 500;
      attr(div17, "class", "value");
      attr(div18, "class", "row");
      attr(div19, "class", "label");
      attr(input6, "type", "checkbox");
      input6.checked = input6_checked_value = !!/*activeRigModeSync*/
      ((_n = (_m = ctx[38]) == null ? void 0 : _m.r2) == null ? void 0 : _n.auto_start);
      attr(div20, "class", "value");
      attr(div21, "class", "row");
      attr(div22, "class", "panel");
      set_style(div22, "margin-top", "12px");
    },
    m(target, anchor) {
      insert(target, div22, anchor);
      append(div22, div0);
      append(div22, t1);
      append(div22, div3);
      append(div3, div1);
      append(div3, t3);
      append(div3, div2);
      append(div2, input0);
      append(div22, t4);
      append(div22, div6);
      append(div6, div4);
      append(div6, t6);
      append(div6, div5);
      append(div5, input1);
      append(div22, t7);
      append(div22, div9);
      append(div9, div7);
      append(div9, t9);
      append(div9, div8);
      append(div8, input2);
      append(div22, t10);
      append(div22, div12);
      append(div12, div10);
      append(div12, t12);
      append(div12, div11);
      append(div11, input3);
      append(div22, t13);
      append(div22, div15);
      append(div15, div13);
      append(div15, t15);
      append(div15, div14);
      append(div14, input4);
      append(div22, t16);
      append(div22, div18);
      append(div18, div16);
      append(div18, t18);
      append(div18, div17);
      append(div17, input5);
      append(div22, t19);
      append(div22, div21);
      append(div21, div19);
      append(div21, t21);
      append(div21, div20);
      append(div20, input6);
      append(div22, t22);
      if (if_block) if_block.m(div22, null);
      if (!mounted) {
        dispose = [
          listen(
            input0,
            "change",
            /*change_handler_26*/
            ctx[208]
          ),
          listen(
            input1,
            "input",
            /*input_handler_8*/
            ctx[209]
          ),
          listen(
            input2,
            "input",
            /*input_handler_9*/
            ctx[210]
          ),
          listen(
            input3,
            "input",
            /*input_handler_10*/
            ctx[211]
          ),
          listen(
            input4,
            "input",
            /*input_handler_11*/
            ctx[212]
          ),
          listen(
            input5,
            "input",
            /*input_handler_12*/
            ctx[213]
          ),
          listen(
            input6,
            "change",
            /*change_handler_27*/
            ctx[214]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n, _o, _p;
      if (dirty[1] & /*activeRigModeSync*/
      128 && input0_checked_value !== (input0_checked_value = !!/*activeRigModeSync*/
      ((_b2 = (_a2 = ctx2[38]) == null ? void 0 : _a2.r2) == null ? void 0 : _b2.enabled))) {
        input0.checked = input0_checked_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input1_value_value !== (input1_value_value = /*activeRigModeSync*/
      ((_d = (_c = ctx2[38]) == null ? void 0 : _c.r2) == null ? void 0 : _d.host) ?? "127.0.0.1") && input1.value !== input1_value_value) {
        input1.value = input1_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input2_value_value !== (input2_value_value = /*activeRigModeSync*/
      ((_f = (_e = ctx2[38]) == null ? void 0 : _e.r2) == null ? void 0 : _f.port) ?? 4533) && input2.value !== input2_value_value) {
        input2.value = input2_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input3_value_value !== (input3_value_value = /*activeRigModeSync*/
      ((_h = (_g = ctx2[38]) == null ? void 0 : _g.r2) == null ? void 0 : _h.connect_timeout_ms) ?? 1500) && input3.value !== input3_value_value) {
        input3.value = input3_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input4_value_value !== (input4_value_value = /*activeRigModeSync*/
      ((_j = (_i = ctx2[38]) == null ? void 0 : _i.r2) == null ? void 0 : _j.io_timeout_ms) ?? 1500) && input4.value !== input4_value_value) {
        input4.value = input4_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input5_value_value !== (input5_value_value = /*activeRigModeSync*/
      ((_l = (_k = ctx2[38]) == null ? void 0 : _k.r2) == null ? void 0 : _l.poll_ms) ?? 500) && input5.value !== input5_value_value) {
        input5.value = input5_value_value;
      }
      if (dirty[1] & /*activeRigModeSync*/
      128 && input6_checked_value !== (input6_checked_value = !!/*activeRigModeSync*/
      ((_n = (_m = ctx2[38]) == null ? void 0 : _m.r2) == null ? void 0 : _n.auto_start))) {
        input6.checked = input6_checked_value;
      }
      if (
        /*activeRigModeSync*/
        (_p = (_o = ctx2[38]) == null ? void 0 : _o.r2) == null ? void 0 : _p.auto_start
      ) {
        if (if_block) {
          if_block.p(ctx2, dirty);
        } else {
          if_block = create_if_block_10(ctx2);
          if_block.c();
          if_block.m(div22, null);
        }
      } else if (if_block) {
        if_block.d(1);
        if_block = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div22);
      }
      if (if_block) if_block.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function create_if_block_10(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_value_value;
  let mounted;
  let dispose;
  return {
    c() {
      var _a, _b;
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "rigctld options:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "class", "input");
      attr(input, "type", "text");
      attr(input, "placeholder", "-m 361 -r /dev/ttyUSB0");
      input.value = input_value_value = /*activeRigModeSync*/
      ((_b = (_a = ctx[38]) == null ? void 0 : _a.r2) == null ? void 0 : _b.rigctld_options) ?? "";
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "input",
          /*input_handler_13*/
          ctx[215]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      var _a, _b;
      if (dirty[1] & /*activeRigModeSync*/
      128 && input_value_value !== (input_value_value = /*activeRigModeSync*/
      ((_b = (_a = ctx2[38]) == null ? void 0 : _a.r2) == null ? void 0 : _b.rigctld_options) ?? "") && input.value !== input_value_value) {
        input.value = input_value_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_8(ctx) {
  let div;
  let t_value = (
    /*rigModeSyncStatus*/
    ctx[9][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*rigModeSyncStatus*/
      ctx[9][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*rigModeSyncStatus, activeSerial*/
      513 && t_value !== (t_value = /*rigModeSyncStatus*/
      ctx2[9][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*rigModeSyncStatus, activeSerial*/
      513 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*rigModeSyncStatus*/
      ctx2[9][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_else_block$1(ctx) {
  let section;
  return {
    c() {
      section = element("section");
      section.innerHTML = `<div class="section-title">Keyer Mode</div> <div class="panel"><div class="placeholder">Keyer Mode is not supported on this device.</div></div>`;
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
      }
    }
  };
}
function create_if_block_1$1(ctx) {
  let section;
  let div0;
  let t1;
  let div4;
  let t2;
  let div3;
  let div1;
  let t4;
  let div2;
  let select;
  let select_value_value;
  let t5;
  let t6;
  let if_block2_anchor;
  let mounted;
  let dispose;
  let if_block0 = (
    /*hasFollowTx*/
    ctx[61] && create_if_block_6(ctx)
  );
  let each_value_1 = ensure_array_like(
    /*keyerModeOptions*/
    ctx[65]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value_1.length; i += 1) {
    each_blocks[i] = create_each_block_1(get_each_context_1(ctx, each_value_1, i));
  }
  let if_block1 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_5$1(ctx)
  );
  let if_block2 = (
    /*hasR2*/
    ctx[3] && create_if_block_2$1(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Keyer Mode Radio 1";
      t1 = space();
      div4 = element("div");
      if (if_block0) if_block0.c();
      t2 = space();
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Current Mode:";
      t4 = space();
      div2 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t5 = space();
      if (if_block1) if_block1.c();
      t6 = space();
      if (if_block2) if_block2.c();
      if_block2_anchor = empty();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div4);
      if (if_block0) if_block0.m(div4, null);
      append(div4, t2);
      append(div4, div3);
      append(div3, div1);
      append(div3, t4);
      append(div3, div2);
      append(div2, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          "r1KeyerMode"
        )
      );
      append(div4, t5);
      if (if_block1) if_block1.m(div4, null);
      insert(target, t6, anchor);
      if (if_block2) if_block2.m(target, anchor);
      insert(target, if_block2_anchor, anchor);
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*change_handler_1*/
          ctx[173]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (
        /*hasFollowTx*/
        ctx2[61]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_6(ctx2);
          if_block0.c();
          if_block0.m(div4, t2);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[2] & /*keyerModeOptions*/
      8) {
        each_value_1 = ensure_array_like(
          /*keyerModeOptions*/
          ctx2[65]
        );
        let i;
        for (i = 0; i < each_value_1.length; i += 1) {
          const child_ctx = get_each_context_1(ctx2, each_value_1, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block_1(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value_1.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select_value_value !== (select_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1KeyerMode"
      ))) {
        select_option(
          select,
          /*keyerParam*/
          ctx2[105](
            /*activeSerial*/
            ctx2[0],
            "r1KeyerMode"
          )
        );
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_5$1(ctx2);
          if_block1.c();
          if_block1.m(div4, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
      if (
        /*hasR2*/
        ctx2[3]
      ) {
        if (if_block2) {
          if_block2.p(ctx2, dirty);
        } else {
          if_block2 = create_if_block_2$1(ctx2);
          if_block2.c();
          if_block2.m(if_block2_anchor.parentNode, if_block2_anchor);
        }
      } else if (if_block2) {
        if_block2.d(1);
        if_block2 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
        detach(t6);
        detach(if_block2_anchor);
      }
      if (if_block0) if_block0.d();
      destroy_each(each_blocks, detaching);
      if (if_block1) if_block1.d();
      if (if_block2) if_block2.d(detaching);
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_6(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Keyer Mode Follows RIG:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r1FollowTxMode"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler*/
          ctx[172]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r1FollowTxMode"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_each_block_1(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_5$1(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block_2$1(ctx) {
  let section;
  let div0;
  let t1;
  let div4;
  let t2;
  let div3;
  let div1;
  let t4;
  let div2;
  let select;
  let select_value_value;
  let t5;
  let mounted;
  let dispose;
  let if_block0 = (
    /*hasFollowTx*/
    ctx[61] && create_if_block_4$1(ctx)
  );
  let each_value = ensure_array_like(
    /*keyerModeOptions*/
    ctx[65]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block(get_each_context(ctx, each_value, i));
  }
  let if_block1 = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ] && create_if_block_3$1(ctx)
  );
  return {
    c() {
      section = element("section");
      div0 = element("div");
      div0.textContent = "Keyer Mode Radio 2";
      t1 = space();
      div4 = element("div");
      if (if_block0) if_block0.c();
      t2 = space();
      div3 = element("div");
      div1 = element("div");
      div1.textContent = "Current Mode:";
      t4 = space();
      div2 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t5 = space();
      if (if_block1) if_block1.c();
      attr(div0, "class", "section-title");
      attr(div1, "class", "label");
      attr(select, "class", "select");
      attr(div2, "class", "value");
      attr(div3, "class", "row");
      attr(div4, "class", "panel");
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
      append(section, div0);
      append(section, t1);
      append(section, div4);
      if (if_block0) if_block0.m(div4, null);
      append(div4, t2);
      append(div4, div3);
      append(div3, div1);
      append(div3, t4);
      append(div3, div2);
      append(div2, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      select_option(
        select,
        /*keyerParam*/
        ctx[105](
          /*activeSerial*/
          ctx[0],
          "r2KeyerMode"
        )
      );
      append(div4, t5);
      if (if_block1) if_block1.m(div4, null);
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*change_handler_3*/
          ctx[175]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (
        /*hasFollowTx*/
        ctx2[61]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
        } else {
          if_block0 = create_if_block_4$1(ctx2);
          if_block0.c();
          if_block0.m(div4, t2);
        }
      } else if (if_block0) {
        if_block0.d(1);
        if_block0 = null;
      }
      if (dirty[2] & /*keyerModeOptions*/
      8) {
        each_value = ensure_array_like(
          /*keyerModeOptions*/
          ctx2[65]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && select_value_value !== (select_value_value = /*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2KeyerMode"
      ))) {
        select_option(
          select,
          /*keyerParam*/
          ctx2[105](
            /*activeSerial*/
            ctx2[0],
            "r2KeyerMode"
          )
        );
      }
      if (
        /*keyerStatus*/
        ctx2[7][
          /*activeSerial*/
          ctx2[0]
        ]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block_3$1(ctx2);
          if_block1.c();
          if_block1.m(div4, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(section);
      }
      if (if_block0) if_block0.d();
      destroy_each(each_blocks, detaching);
      if (if_block1) if_block1.d();
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_4$1(ctx) {
  let div2;
  let div0;
  let t1;
  let div1;
  let input;
  let input_checked_value;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.textContent = "Keyer Mode Follows RIG:";
      t1 = space();
      div1 = element("div");
      input = element("input");
      attr(div0, "class", "label");
      attr(input, "type", "checkbox");
      input.checked = input_checked_value = !!/*keyerParam*/
      ctx[105](
        /*activeSerial*/
        ctx[0],
        "r2FollowTxMode"
      );
      attr(div1, "class", "value");
      attr(div2, "class", "row");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t1);
      append(div2, div1);
      append(div1, input);
      if (!mounted) {
        dispose = listen(
          input,
          "change",
          /*change_handler_2*/
          ctx[174]
        );
        mounted = true;
      }
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*activeSerial*/
      1 | dirty[1] & /*activeAntsAndGroups*/
      8192 && input_checked_value !== (input_checked_value = !!/*keyerParam*/
      ctx2[105](
        /*activeSerial*/
        ctx2[0],
        "r2FollowTxMode"
      ))) {
        input.checked = input_checked_value;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_each_block(ctx) {
  let option;
  let t_value = (
    /*opt*/
    ctx[489].label + ""
  );
  let t;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      option.__value = /*opt*/
      ctx[489].value;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_if_block_3$1(ctx) {
  let div;
  let t_value = (
    /*keyerStatus*/
    ctx[7][
      /*activeSerial*/
      ctx[0]
    ].text + ""
  );
  let t;
  let div_class_value;
  return {
    c() {
      div = element("div");
      t = text(t_value);
      attr(div, "class", div_class_value = `inline-status ${/*keyerStatus*/
      ctx[7][
        /*activeSerial*/
        ctx[0]
      ].kind === "error" ? "error" : ""}`);
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 && t_value !== (t_value = /*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].text + "")) set_data(t, t_value);
      if (dirty[0] & /*keyerStatus, activeSerial*/
      129 | dirty[1] & /*activeAntsAndGroups*/
      8192 && div_class_value !== (div_class_value = `inline-status ${/*keyerStatus*/
      ctx2[7][
        /*activeSerial*/
        ctx2[0]
      ].kind === "error" ? "error" : ""}`)) {
        attr(div, "class", div_class_value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_fragment$1(ctx) {
  let if_block_anchor;
  function select_block_type(ctx2, dirty) {
    if (
      /*activeMenuId*/
      ctx2[1] === "mode"
    ) return create_if_block$1;
    if (
      /*activeMenuId*/
      ctx2[1] === "radio"
    ) return create_if_block_7;
    if (
      /*activeMenuId*/
      ctx2[1] === "aux"
    ) return create_if_block_18;
    if (
      /*activeMenuId*/
      ctx2[1] === "fsk"
    ) return create_if_block_21;
    if (
      /*activeMenuId*/
      ctx2[1] === "ptt"
    ) return create_if_block_29;
    if (
      /*activeMenuId*/
      ctx2[1] === "audio"
    ) return create_if_block_44;
    if (
      /*activeMenuId*/
      ctx2[1] === "display"
    ) return create_if_block_48;
    if (
      /*activeMenuId*/
      ctx2[1] === "cw"
    ) return create_if_block_49;
    if (
      /*activeMenuId*/
      ctx2[1] === "cw_messages"
    ) return create_if_block_52;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_load_store"
    ) return create_if_block_55;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_settings"
    ) return create_if_block_57;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_outputs"
    ) return create_if_block_60;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_ant_list"
    ) return create_if_block_62;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_vr_list"
    ) return create_if_block_69;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_grp_list"
    ) return create_if_block_81;
    if (
      /*activeMenuId*/
      ctx2[1] === "sm_antsw_band_list"
    ) return create_if_block_93;
    if (
      /*activeMenuId*/
      ctx2[1] === "all_params"
    ) return create_if_block_119;
    return create_else_block_22;
  }
  function select_block_ctx(ctx2, type) {
    if (type === create_if_block_62) return get_if_ctx_2(ctx2);
    if (type === create_if_block_69) return get_if_ctx_5(ctx2);
    if (type === create_if_block_81) return get_if_ctx_8(ctx2);
    if (type === create_if_block_93) return get_if_ctx_11(ctx2);
    return ctx2;
  }
  let current_block_type = select_block_type(ctx);
  let if_block = current_block_type(select_block_ctx(ctx, current_block_type));
  return {
    c() {
      if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type(ctx2)) && if_block) {
        if_block.p(select_block_ctx(ctx2, current_block_type), dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(select_block_ctx(ctx2, current_block_type));
        if (if_block) {
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if_block.d(detaching);
    }
  };
}
function instance$1($$self, $$props, $$invalidate) {
  let hasKeyerMode;
  let hasR2;
  let hasAux;
  let hasFsk1;
  let hasFsk2;
  let hasPfsk;
  let hasFollowTx;
  let hasLnaPaPtt;
  let hasLnaPaPttTail;
  let hasSoundcardPtt;
  let hasCwInVoice;
  let isAudioMk1;
  let isAudioMk2;
  let isAudioMk2R;
  let activeAnts;
  let activeAntOutCols;
  let activeVrs;
  let activeGrps;
  let activeAllObjs;
  let activeBnds;
  let activeBpfOutputs;
  let activeSeqOutputs;
  let activeAntOutputs;
  let activeAntsAndGroups;
  let activeR1;
  let activeR2;
  let activeAux;
  let activeFsk1;
  let activeFsk2;
  let activeRigModeSync;
  let activePttR1;
  let activePttR2;
  const playMessage = (serial, messageNum) => {
    wsSend({
      type: "command",
      command: "play_message",
      serial,
      message: messageNum
    });
  };
  let { activeSerial = "" } = $$props;
  let { activeMenuId = "" } = $$props;
  let { activeKeyer = null } = $$props;
  let { activeKeyerFlags = [] } = $$props;
  let { configDevices = [] } = $$props;
  let { metadata = null } = $$props;
  let { keyers = [] } = $$props;
  let { refreshDeviceConfig = async () => {
  } } = $$props;
  let keyerStatus = {};
  let keyerStatusTimer = {};
  let radioForm = {};
  let radioFormGen = 0;
  let radioStatus = {};
  let radioStatusTimer = {};
  let rigModeSyncForm = {};
  let rigModeSyncStatus = {};
  let rigModeSyncStatusTimer = {};
  let pttForm = {};
  let pttFormGen = 0;
  let pttStatus = {};
  let pttStatusTimer = {};
  let messageForm = {};
  let messageFormGen = 0;
  let messageStatus = {};
  let messageStatusTimer = {};
  let allParamForm = {};
  let allParamFormGen = 0;
  let allParamStatus = {};
  let allParamStatusTimer = {};
  let smActionStatus = {};
  let smActionStatusTimer = {};
  let smActionBusy = {};
  let smAntMode = {};
  let smAntEditForms = {};
  let smAntAddForm = {};
  let smAntSelected = {};
  let smVrMode = {};
  let smVrAddForm = {};
  let smVrEditForms = {};
  let smVrSelected = {};
  let smVrAddAnt = {};
  let smGrpMode = {};
  let smGrpAddForm = {};
  let smGrpEditForms = {};
  let smGrpSelected = {};
  let smGrpAddAnt = {};
  let smBndMode = {};
  let smBndAddForm = {};
  let smBndEditForms = {};
  let smBndSelected = {};
  let smBndAddRef = {};
  const keyerModeOptions = [
    { value: 0, label: "CW" },
    { value: 1, label: "VOICE" },
    { value: 2, label: "FSK" },
    { value: 3, label: "DIGITAL" }
  ];
  const radioBaudOptions = [1200, 2400, 4800, 9600, 19200, 38400, 57600];
  const radioDataBitsOptions = [8, 7, 6, 5];
  const radioStopBitsOptions = [1, 2];
  const fskBaudOptions = [22, 45, 45.45, 50, 56.25, 75, 100, 150, 300];
  const fskDataBitsOptions = [8, 7, 6, 5];
  const fskStopBitsOptions = [1, 1.5, 2];
  const audioOptions = [
    { value: 2, label: "A (Mic -> Radio Mic)" },
    {
      value: 1,
      label: "B (Soundcard -> Radio Line)"
    },
    {
      value: 3,
      label: "C (Soundcard -> Radio Mic)"
    },
    { value: 0, label: "D (like A + B)" }
  ];
  const audioOptionsByType = {
    4: {
      cw: [2],
      voice: [1, 2, 3],
      digital: [1, 2, 3]
    },
    5: {
      cw: [2],
      voice: [0, 1, 2, 3],
      digital: [0, 1, 2, 3]
    },
    11: {
      cw: [2],
      voice: [0, 1, 2, 3],
      digital: [0, 1, 2, 3]
    },
    6: {
      cw: [2],
      voice: [1, 2, 3],
      digital: [1, 2, 3]
    },
    7: {
      cw: [2],
      voice: [1, 2, 3],
      digital: [1, 2, 3]
    }
  };
  const mk2MicSelOptions = [
    { value: "auto", label: "Auto" },
    { value: "front", label: "Front" },
    { value: "rear", label: "Rear" }
  ];
  const mk2rCodecOptions = [{ value: 1, label: "SC1" }, { value: 0, label: "SC2" }];
  const winkeyModeOptions = [
    { value: 0, label: "IambicB" },
    { value: 1, label: "IambicA" },
    { value: 2, label: "Ultimatic" },
    { value: 3, label: "Bug Keyer" }
  ];
  const winkeyUltimaticOptions = [
    { value: 0, label: "Normal" },
    { value: 1, label: "Dah" },
    { value: 2, label: "Dit" }
  ];
  const sideToneOptions = [
    { value: 0, label: "Off" },
    { value: 1, label: "1350 Hz" },
    { value: 2, label: "675 Hz" },
    { value: 3, label: "450 Hz" },
    { value: 4, label: "338 Hz" }
  ];
  const displayLineOptions = [{ value: 0, label: "Upper Line" }, { value: 1, label: "Lower Line" }];
  const messageSlots = [1, 2, 3, 4, 5, 6, 7, 8];
  const pttOptionLabels = {
    ptt: "PTT",
    ptt1: "PTT1 (mic jack)",
    ptt2: "PTT2 (rear panel jack)",
    ptt12: "PTT1+2 (both)",
    qsk: "QSK",
    semi: "Semi Break-in",
    noptt: "No PTT"
  };
  const pttOptionsByType = {
    1: { cw: ["ptt", "qsk"] },
    2: { cw: ["ptt", "noptt"] },
    3: { cw: ["ptt", "qsk", "semi"] },
    4: {
      cw: ["ptt1", "qsk", "semi", "ptt2"],
      voice: ["ptt1", "ptt2"],
      digital: ["ptt1", "ptt2", "ptt12"]
    },
    5: {
      cw: ["ptt1", "qsk", "semi", "ptt2"],
      voice: ["ptt1", "ptt2"],
      digital: ["ptt1", "ptt2", "ptt12"]
    },
    6: {
      cw: ["ptt1", "qsk", "semi", "ptt2"],
      voice: ["ptt1", "ptt2"],
      digital: ["ptt1", "ptt2", "ptt12"]
    },
    7: {
      cw: ["ptt1", "qsk", "semi", "ptt2"],
      voice: ["ptt1", "ptt2"],
      digital: ["ptt1", "ptt2", "ptt12"]
    },
    11: {
      cw: ["ptt1", "qsk", "semi", "ptt2"],
      voice: ["ptt1", "ptt2"],
      digital: ["ptt1", "ptt2", "ptt12"]
    }
  };
  const digitalOverVoiceOptions = [
    { value: 0, label: "Band Map" },
    { value: 1, label: "Always VOICE" },
    { value: 2, label: "Always DIGITAL" }
  ];
  const smCivFuncOptions = [
    { value: 0, label: "none" },
    { value: 1, label: "TX frequency" },
    { value: 2, label: "RX frequency" },
    { value: 3, label: "operation frequency" },
    { value: 4, label: "VFO A frequency" },
    { value: 5, label: "VFO B frequency" },
    { value: 6, label: "sub RX frequency" }
  ];
  const smCivBaudOptions = [
    { value: 192, label: "1200" },
    { value: 96, label: "2400" },
    { value: 48, label: "4800" },
    { value: 24, label: "9600" },
    { value: 12, label: "19200" }
  ];
  const smExtSerFuncOptions = [
    { value: 0, label: "none" },
    { value: 1, label: "auxiliary port" },
    { value: 2, label: "Acom 2000" },
    { value: 17, label: "CI-V TX frequency" },
    { value: 18, label: "CI-V RX frequency" },
    {
      value: 19,
      label: "CI-V operation frequency"
    },
    { value: 20, label: "CI-V VFO A frequency" },
    { value: 21, label: "CI-V VFO B frequency" },
    {
      value: 22,
      label: "CI-V sub RX frequency"
    },
    { value: 33, label: "SteppIR Dipole" },
    {
      value: 34,
      label: "SteppIR Dipole with 40m-30m Dipole Adder"
    },
    { value: 35, label: "SteppIR Yagi" },
    {
      value: 36,
      label: "SteppIR Yagi with 40m-30m Dipole Adder"
    },
    {
      value: 37,
      label: "SteppIR BigIR Vertical"
    },
    {
      value: 38,
      label: "SteppIR BigIR Vertical with 80m Coil"
    },
    {
      value: 39,
      label: "SteppIR SmallIR Vertical"
    },
    { value: 40, label: "SteppIR MonstIR Yagi" }
  ];
  const smOutputClassOptions = [
    { value: 0, label: "ANT" },
    { value: 1, label: "BPF" },
    { value: 2, label: "SEQ" },
    { value: 3, label: "SEQ inverted" }
  ];
  const smOutputNames = [
    "A1",
    "A2",
    "A3",
    "A4",
    "A5",
    "A6",
    "A7",
    "A8",
    "A9",
    "A10",
    "B1",
    "B2",
    "B3",
    "B4",
    "B5",
    "B6",
    "B7",
    "B8",
    "B9",
    "B10"
  ];
  const keyerFlagsForSerial = (serial) => {
    const cfg = keyers.find((k) => k.serial === serial);
    if (!cfg || cfg.type == null || !(metadata == null ? void 0 : metadata.devicetypes)) return [];
    const devType = metadata.devicetypes.find((d) => d.type === cfg.type);
    return (devType == null ? void 0 : devType.flags) || [];
  };
  const hasFlag = (serial, flag) => keyerFlagsForSerial(serial).includes(flag);
  const keyerConfigForSerial = (serial) => configDevices.find((d) => d.serial === serial) || {};
  const channelConfig = (serial, chan) => {
    const cfg = keyerConfigForSerial(serial);
    if (!(cfg == null ? void 0 : cfg.channel) || !cfg.channel[chan]) return {};
    return cfg.channel[chan];
  };
  const defaultRadioForm = (serial, chan) => {
    const ch = channelConfig(serial, chan);
    const isFsk = chan === "fsk1" || chan === "fsk2";
    return {
      baud: ch.baud ?? (isFsk ? 45.45 : 9600),
      databits: ch.databits ?? (isFsk ? 5 : 8),
      stopbits: ch.stopbits ?? (isFsk ? 1.5 : 1),
      rtscts: ch.rtscts ?? 0,
      rigtype: ch.rigtype ?? 0,
      icomaddress: ch.icomaddress ?? 0,
      icomsimulateautoinfo: ch.icomsimulateautoinfo ?? 0,
      digitalovervoicerule: ch.digitalovervoicerule ?? 0,
      usedecoderifconnected: ch.usedecoderifconnected ?? 0,
      dontinterfereusbcontrol: ch.dontinterfereusbcontrol ?? 0
    };
  };
  const ensureRadioForm = (serial, chan) => {
    if (!serial) return;
    const perSerial = radioForm[serial] || {};
    if (!perSerial[chan]) {
      $$invalidate(165, radioForm = {
        ...radioForm,
        [serial]: {
          ...perSerial,
          [chan]: defaultRadioForm(serial, chan)
        }
      });
    }
  };
  const resetRadioForms = () => {
    $$invalidate(165, radioForm = {});
    $$invalidate(166, radioFormGen++, radioFormGen);
  };
  const updateRadioForm = (serial, chan, key, value) => {
    const perSerial = radioForm[serial] || {};
    const perChan = perSerial[chan] || defaultRadioForm(serial, chan);
    $$invalidate(165, radioForm = {
      ...radioForm,
      [serial]: {
        ...perSerial,
        [chan]: { ...perChan, [key]: value }
      }
    });
  };
  const boolish = (v) => !!Number(v || 0) || v === true;
  const defaultRigModeSyncForm = (serial) => {
    var _a, _b, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n, _o, _p;
    const cfg = keyerConfigForSerial(serial);
    const src = (cfg == null ? void 0 : cfg.rig_mode_sync) || {};
    const radios = src.radios || {};
    return {
      backend: src.backend ?? "rigctld",
      enabled: boolish(src.enabled),
      r1: {
        enabled: boolish((_a = radios == null ? void 0 : radios.r1) == null ? void 0 : _a.enabled),
        host: ((_b = radios == null ? void 0 : radios.r1) == null ? void 0 : _b.host) ?? "127.0.0.1",
        port: Number(((_c = radios == null ? void 0 : radios.r1) == null ? void 0 : _c.port) ?? 4532),
        connect_timeout_ms: Number(((_d = radios == null ? void 0 : radios.r1) == null ? void 0 : _d.connect_timeout_ms) ?? 1500),
        io_timeout_ms: Number(((_e = radios == null ? void 0 : radios.r1) == null ? void 0 : _e.io_timeout_ms) ?? 1500),
        poll_ms: Number(((_f = radios == null ? void 0 : radios.r1) == null ? void 0 : _f.poll_ms) ?? 500),
        auto_start: boolish((_g = radios == null ? void 0 : radios.r1) == null ? void 0 : _g.auto_start),
        rigctld_options: ((_h = radios == null ? void 0 : radios.r1) == null ? void 0 : _h.rigctld_options) ?? ""
      },
      r2: {
        enabled: boolish((_i = radios == null ? void 0 : radios.r2) == null ? void 0 : _i.enabled),
        host: ((_j = radios == null ? void 0 : radios.r2) == null ? void 0 : _j.host) ?? "127.0.0.1",
        port: Number(((_k = radios == null ? void 0 : radios.r2) == null ? void 0 : _k.port) ?? 4533),
        connect_timeout_ms: Number(((_l = radios == null ? void 0 : radios.r2) == null ? void 0 : _l.connect_timeout_ms) ?? 1500),
        io_timeout_ms: Number(((_m = radios == null ? void 0 : radios.r2) == null ? void 0 : _m.io_timeout_ms) ?? 1500),
        poll_ms: Number(((_n = radios == null ? void 0 : radios.r2) == null ? void 0 : _n.poll_ms) ?? 500),
        auto_start: boolish((_o = radios == null ? void 0 : radios.r2) == null ? void 0 : _o.auto_start),
        rigctld_options: ((_p = radios == null ? void 0 : radios.r2) == null ? void 0 : _p.rigctld_options) ?? ""
      }
    };
  };
  const ensureRigModeSyncForm = (serial) => {
    if (!serial) return;
    if (!rigModeSyncForm[serial]) {
      $$invalidate(167, rigModeSyncForm = {
        ...rigModeSyncForm,
        [serial]: defaultRigModeSyncForm(serial)
      });
    }
  };
  const resetRigModeSyncForms = () => {
    $$invalidate(167, rigModeSyncForm = {});
  };
  const updateRigModeSyncTop = (serial, key, value) => {
    const form = rigModeSyncForm[serial] || defaultRigModeSyncForm(serial);
    $$invalidate(167, rigModeSyncForm = {
      ...rigModeSyncForm,
      [serial]: { ...form, [key]: value }
    });
  };
  const updateRigModeSyncRadio = (serial, radio, key, value) => {
    const form = rigModeSyncForm[serial] || defaultRigModeSyncForm(serial);
    const cur = form[radio] || {};
    $$invalidate(167, rigModeSyncForm = {
      ...rigModeSyncForm,
      [serial]: {
        ...form,
        [radio]: { ...cur, [key]: value }
      }
    });
  };
  const setRigModeSyncStatusMsg = (serial, msg, kind = "success") => {
    $$invalidate(9, rigModeSyncStatus = {
      ...rigModeSyncStatus,
      [serial]: { text: msg, kind }
    });
    if (rigModeSyncStatusTimer[serial]) clearTimeout(rigModeSyncStatusTimer[serial]);
    rigModeSyncStatusTimer[serial] = setTimeout(
      () => {
        const { [serial]: _removed, ...rest } = rigModeSyncStatus;
        $$invalidate(9, rigModeSyncStatus = rest);
      },
      3e3
    );
  };
  const applyRigModeSync = async (serial) => {
    var _a, _b, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n, _o, _p;
    const form = rigModeSyncForm[serial] || defaultRigModeSyncForm(serial);
    const payload = {
      backend: form.backend || "rigctld",
      enabled: form.enabled ? 1 : 0,
      radios: {
        r1: {
          enabled: ((_a = form.r1) == null ? void 0 : _a.enabled) ? 1 : 0,
          host: String(((_b = form.r1) == null ? void 0 : _b.host) || "127.0.0.1").trim() || "127.0.0.1",
          port: Number(((_c = form.r1) == null ? void 0 : _c.port) || 4532),
          connect_timeout_ms: Number(((_d = form.r1) == null ? void 0 : _d.connect_timeout_ms) || 1500),
          io_timeout_ms: Number(((_e = form.r1) == null ? void 0 : _e.io_timeout_ms) || 1500),
          poll_ms: Number(((_f = form.r1) == null ? void 0 : _f.poll_ms) || 500),
          auto_start: ((_g = form.r1) == null ? void 0 : _g.auto_start) ? 1 : 0,
          rigctld_options: String(((_h = form.r1) == null ? void 0 : _h.rigctld_options) || "")
        }
      }
    };
    if (hasR2) {
      payload.radios.r2 = {
        enabled: ((_i = form.r2) == null ? void 0 : _i.enabled) ? 1 : 0,
        host: String(((_j = form.r2) == null ? void 0 : _j.host) || "127.0.0.1").trim() || "127.0.0.1",
        port: Number(((_k = form.r2) == null ? void 0 : _k.port) || 4533),
        connect_timeout_ms: Number(((_l = form.r2) == null ? void 0 : _l.connect_timeout_ms) || 1500),
        io_timeout_ms: Number(((_m = form.r2) == null ? void 0 : _m.io_timeout_ms) || 1500),
        poll_ms: Number(((_n = form.r2) == null ? void 0 : _n.poll_ms) || 500),
        auto_start: ((_o = form.r2) == null ? void 0 : _o.auto_start) ? 1 : 0,
        rigctld_options: String(((_p = form.r2) == null ? void 0 : _p.rigctld_options) || "")
      };
    }
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, rig_mode_sync: payload })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(167, rigModeSyncForm = {
        ...rigModeSyncForm,
        [serial]: defaultRigModeSyncForm(serial)
      });
      setRigModeSyncStatusMsg(serial, "Rig mode sync settings applied.", "success");
    } catch (err) {
      setRigModeSyncStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to apply rig mode sync settings.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const messageConfig = (serial, kind) => {
    const cfg = keyerConfigForSerial(serial);
    if (!cfg) return [];
    return kind === "cw" ? cfg.cwMessages || [] : cfg.fskMessages || [];
  };
  const defaultMessageForm = (serial, kind) => {
    const map = {};
    messageSlots.forEach((idx) => {
      map[idx] = "";
    });
    const items = messageConfig(serial, kind);
    items.forEach((msg) => {
      if (msg == null ? void 0 : msg.index) map[msg.index] = msg.text ?? "";
    });
    return map;
  };
  const ensureMessageForm = (serial, kind) => {
    if (!serial) return;
    const perSerial = messageForm[serial] || {};
    if (!perSerial[kind]) {
      $$invalidate(11, messageForm = {
        ...messageForm,
        [serial]: {
          ...perSerial,
          [kind]: defaultMessageForm(serial, kind)
        }
      });
    }
  };
  const resetMessageForms = () => {
    $$invalidate(11, messageForm = {});
    $$invalidate(170, messageFormGen++, messageFormGen);
  };
  const updateMessageForm = (serial, kind, index, value) => {
    const perSerial = messageForm[serial] || {};
    const perKind = perSerial[kind] || defaultMessageForm(serial, kind);
    $$invalidate(11, messageForm = {
      ...messageForm,
      [serial]: {
        ...perSerial,
        [kind]: { ...perKind, [index]: value }
      }
    });
  };
  const handleMessageInput = (serial, kind, index, target) => {
    const upper = (target.value ?? "").toUpperCase();
    if (upper !== target.value) {
      const start = target.selectionStart;
      const end = target.selectionEnd;
      target.value = upper;
      if (start !== null) target.setSelectionRange(start, end);
    }
    updateMessageForm(serial, kind, index, upper);
  };
  const flattenParamEntries = (param = {}) => {
    const entries = [];
    Object.entries(param || {}).forEach(([key, value]) => {
      if (value && typeof value === "object" && !Array.isArray(value)) {
        Object.entries(value).forEach(([subKey, subVal]) => {
          entries.push({ key: `${key}.${subKey}`, value: subVal });
        });
      } else {
        entries.push({ key, value });
      }
    });
    return entries;
  };
  const defaultAllParamForm = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const param = (cfg == null ? void 0 : cfg.param) || {};
    const entries = flattenParamEntries(param);
    const map = {};
    entries.forEach((entry) => {
      map[entry.key] = entry.value;
    });
    return map;
  };
  const ensureAllParamForm = (serial) => {
    if (!serial) return;
    if (!allParamForm[serial]) {
      $$invalidate(13, allParamForm = {
        ...allParamForm,
        [serial]: defaultAllParamForm(serial)
      });
    }
  };
  const resetAllParamForms = () => {
    $$invalidate(13, allParamForm = {});
    $$invalidate(171, allParamFormGen++, allParamFormGen);
  };
  const updateAllParamForm = (serial, key, value) => {
    const perSerial = allParamForm[serial] || defaultAllParamForm(serial);
    $$invalidate(13, allParamForm = {
      ...allParamForm,
      [serial]: { ...perSerial, [key]: value }
    });
  };
  const setAllParamStatusMsg = (serial, msg, kind = "success") => {
    $$invalidate(14, allParamStatus = {
      ...allParamStatus,
      [serial]: { text: msg, kind }
    });
    if (allParamStatusTimer[serial]) clearTimeout(allParamStatusTimer[serial]);
    allParamStatusTimer[serial] = setTimeout(
      () => {
        const { [serial]: _removed, ...rest } = allParamStatus;
        $$invalidate(14, allParamStatus = rest);
      },
      3e3
    );
  };
  const applyAllParams = async (serial) => {
    const perSerial = allParamForm[serial] || defaultAllParamForm(serial);
    const payload = { ...perSerial };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, param: payload })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(13, allParamForm = {
        ...allParamForm,
        [serial]: defaultAllParamForm(serial)
      });
      setAllParamStatusMsg(serial, "All parameters applied.", "success");
    } catch (err) {
      setAllParamStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to apply parameters.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const setMessageStatusMsg = (serial, kind, msg, statusKind = "success") => {
    const key = `${serial}:${kind}`;
    $$invalidate(12, messageStatus = {
      ...messageStatus,
      [key]: { text: msg, kind: statusKind }
    });
    if (messageStatusTimer[key]) clearTimeout(messageStatusTimer[key]);
    messageStatusTimer[key] = setTimeout(
      () => {
        const { [key]: _removed, ...rest } = messageStatus;
        $$invalidate(12, messageStatus = rest);
      },
      3e3
    );
  };
  const applyMessages = async (serial, kind) => {
    const perSerial = messageForm[serial] || {};
    const perKind = perSerial[kind] || defaultMessageForm(serial, kind);
    const payload = messageSlots.map((idx) => ({
      index: idx,
      text: (perKind[idx] ?? "").toUpperCase()
    }));
    const key = kind === "cw" ? "cwMessages" : "fskMessages";
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, [key]: payload })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(11, messageForm = {
        ...messageForm,
        [serial]: {
          ...messageForm[serial],
          [kind]: defaultMessageForm(serial, kind)
        }
      });
      setMessageStatusMsg(serial, kind, `${kind.toUpperCase()} messages applied.`, "success");
    } catch (err) {
      setMessageStatusMsg(serial, kind, (err == null ? void 0 : err.message) || `Failed to apply ${kind.toUpperCase()} messages.`, "error");
      await refreshDeviceConfig(serial);
    }
  };
  const audioOptionsFor = (type, mode) => {
    var _a;
    const list = (_a = audioOptionsByType[type]) == null ? void 0 : _a[mode];
    return list || [1, 2, 3];
  };
  const audioOptionLabel = (value) => {
    var _a;
    return ((_a = audioOptions.find((o) => o.value === value)) == null ? void 0 : _a.label) || value;
  };
  const mk2MicSelValue = (serial) => {
    if (keyerParam(serial, "micSelAuto")) return "auto";
    if (keyerParam(serial, "micSelFront")) return "front";
    return "rear";
  };
  const displayBackgroundOptions = (type) => {
    if (!(metadata == null ? void 0 : metadata.devicetypes)) return [];
    const devType = metadata.devicetypes.find((d) => d.type === type);
    return (devType == null ? void 0 : devType.displaybackground) || [];
  };
  const displayEventOptions = (type) => {
    if (!(metadata == null ? void 0 : metadata.devicetypes)) return [];
    const devType = metadata.devicetypes.find((d) => d.type === type);
    return (devType == null ? void 0 : devType.displayevent) || [];
  };
  const pttOptionsFor = (type, mode) => {
    const byType = pttOptionsByType[type];
    if (byType) return byType[mode] || [];
    if (mode === "cw") return ["ptt1", "ptt2", "ptt12", "qsk", "semi", "ptt", "noptt"];
    return ["ptt1", "ptt2", "ptt12"];
  };
  const pttConfigForSerial = (serial) => {
    var _a;
    return ((_a = keyerConfigForSerial(serial)) == null ? void 0 : _a.ptt) || {};
  };
  const defaultPttForm = (serial, chan) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    const type = (cfg == null ? void 0 : cfg.type) ?? 0;
    const ptt = ((_a = pttConfigForSerial(serial)) == null ? void 0 : _a[chan]) || {};
    const cwOpts = pttOptionsFor(type, "cw");
    const voiceOpts = pttOptionsFor(type, "voice");
    const digitalOpts = pttOptionsFor(type, "digital");
    return {
      cw: ptt.cw ?? cwOpts[0] ?? "ptt1",
      voice: voiceOpts.length ? ptt.voice ?? voiceOpts[0] : null,
      digital: digitalOpts.length ? ptt.digital ?? digitalOpts[0] : null
    };
  };
  const ensurePttForm = (serial, chan) => {
    if (!serial) return;
    const perSerial = pttForm[serial] || {};
    if (!perSerial[chan]) {
      $$invalidate(168, pttForm = {
        ...pttForm,
        [serial]: {
          ...perSerial,
          [chan]: defaultPttForm(serial, chan)
        }
      });
    }
  };
  const resetPttForms = () => {
    $$invalidate(168, pttForm = {});
    $$invalidate(169, pttFormGen++, pttFormGen);
  };
  const updatePttForm = (serial, chan, key, value) => {
    const perSerial = pttForm[serial] || {};
    const perChan = perSerial[chan] || defaultPttForm(serial, chan);
    $$invalidate(168, pttForm = {
      ...pttForm,
      [serial]: {
        ...perSerial,
        [chan]: { ...perChan, [key]: value }
      }
    });
  };
  const setPttStatusMsg = (serial, chan, msg, kind = "success") => {
    const key = `${serial}:${chan}`;
    $$invalidate(10, pttStatus = { ...pttStatus, [key]: { text: msg, kind } });
    if (pttStatusTimer[key]) clearTimeout(pttStatusTimer[key]);
    pttStatusTimer[key] = setTimeout(
      () => {
        const { [key]: _removed, ...rest } = pttStatus;
        $$invalidate(10, pttStatus = rest);
      },
      3e3
    );
  };
  const applyPttChannel = async (serial, chan) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    const type = (cfg == null ? void 0 : cfg.type) ?? 0;
    const form = ((_a = pttForm[serial]) == null ? void 0 : _a[chan]) || defaultPttForm(serial, chan);
    pttOptionsFor(type, "cw");
    const voiceOpts = pttOptionsFor(type, "voice");
    const digitalOpts = pttOptionsFor(type, "digital");
    const payload = { cw: form.cw };
    if (voiceOpts.length && form.voice) payload.voice = form.voice;
    if (digitalOpts.length && form.digital) payload.digital = form.digital;
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, ptt: { [chan]: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(168, pttForm = {
        ...pttForm,
        [serial]: {
          ...pttForm[serial],
          [chan]: defaultPttForm(serial, chan)
        }
      });
      setPttStatusMsg(serial, chan, "PTT settings applied.", "success");
    } catch (err) {
      setPttStatusMsg(serial, chan, (err == null ? void 0 : err.message) || "Failed to apply PTT settings.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const applyPttChange = async (serial, chan, key, value) => {
    updatePttForm(serial, chan, key, value);
    await applyPttChannel(serial, chan);
  };
  const setRadioStatusMsg = (serial, chan, msg, kind = "success") => {
    const key = `${serial}:${chan}`;
    $$invalidate(8, radioStatus = {
      ...radioStatus,
      [key]: { text: msg, kind }
    });
    if (radioStatusTimer[key]) clearTimeout(radioStatusTimer[key]);
    radioStatusTimer[key] = setTimeout(
      () => {
        const { [key]: _removed, ...rest } = radioStatus;
        $$invalidate(8, radioStatus = rest);
      },
      3e3
    );
  };
  const applyRadioChannel = async (serial, chan) => {
    var _a;
    const form = ((_a = radioForm[serial]) == null ? void 0 : _a[chan]) || defaultRadioForm(serial, chan);
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({
          serial,
          channel: {
            [chan]: {
              baud: Number(form.baud),
              databits: Number(form.databits),
              stopbits: Number(form.stopbits),
              rtscts: Number(form.rtscts),
              rigtype: Number(form.rigtype),
              icomaddress: Number(form.icomaddress),
              icomsimulateautoinfo: Number(form.icomsimulateautoinfo),
              digitalovervoicerule: Number(form.digitalovervoicerule),
              usedecoderifconnected: Number(form.usedecoderifconnected),
              dontinterfereusbcontrol: Number(form.dontinterfereusbcontrol)
            }
          }
        })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(165, radioForm = {
        ...radioForm,
        [serial]: {
          ...radioForm[serial],
          [chan]: defaultRadioForm(serial, chan)
        }
      });
      setRadioStatusMsg(serial, chan, "Radio settings applied.", "success");
    } catch (err) {
      setRadioStatusMsg(serial, chan, (err == null ? void 0 : err.message) || "Failed to apply radio settings.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const rigtypes = () => (metadata == null ? void 0 : metadata.rigtypes) || [];
  const setRigType = (serial, chan, rigId) => {
    const rig = rigtypes().find((r) => r.id === Number(rigId));
    const icomAddr = (rig == null ? void 0 : rig.icomAddr) ?? 0;
    updateRadioForm(serial, chan, "rigtype", Number(rigId));
    updateRadioForm(serial, chan, "icomaddress", icomAddr);
  };
  const keyerParam = (serial, key, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    if (!(cfg == null ? void 0 : cfg.param) || cfg.param[key] == null) return fallback;
    return cfg.param[key];
  };
  const setKeyerStatusMsg = (serial, msg, kind = "success") => {
    $$invalidate(7, keyerStatus = {
      ...keyerStatus,
      [serial]: { text: msg, kind }
    });
    if (keyerStatusTimer[serial]) clearTimeout(keyerStatusTimer[serial]);
    keyerStatusTimer[serial] = setTimeout(
      () => {
        const { [serial]: _removed, ...rest } = keyerStatus;
        $$invalidate(7, keyerStatus = rest);
      },
      3e3
    );
  };
  const updateKeyerParam = async (serial, paramKey, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, param: { [paramKey]: value } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, "Keyer mode updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update keyer mode.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const updateWinkeyParam = async (serial, key, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, winkey: { [key]: value } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, "Winkey settings updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update winkey settings.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const winkeyValue = (serial, key, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    if (!(cfg == null ? void 0 : cfg.winkey) || cfg.winkey[key] == null) return fallback;
    return cfg.winkey[key];
  };
  const updateKeyerParams = async (serial, params, msg = "Settings updated.") => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, param: params })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, msg, "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update settings.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const setSmActionStatus = (serial, msg, kind = "success") => {
    $$invalidate(15, smActionStatus = {
      ...smActionStatus,
      [serial]: { text: msg, kind }
    });
    if (smActionStatusTimer[serial]) clearTimeout(smActionStatusTimer[serial]);
    smActionStatusTimer[serial] = setTimeout(
      () => {
        const { [serial]: _removed, ...rest } = smActionStatus;
        $$invalidate(15, smActionStatus = rest);
      },
      5e3
    );
  };
  const smAction = async (serial, action) => {
    $$invalidate(16, smActionBusy = { ...smActionBusy, [serial]: true });
    try {
      const res = await fetch(`/api/v1/devices/${serial}/actions`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ action })
      });
      const result = await res.json();
      if (!res.ok || result.status === "error") {
        setSmActionStatus(serial, result.message || "Action failed.", "error");
      } else {
        setSmActionStatus(serial, result.message || "Action completed.", "success");
      }
    } catch (err) {
      setSmActionStatus(serial, (err == null ? void 0 : err.message) || "Action failed.", "error");
    } finally {
      const { [serial]: _removed, ...rest } = smActionBusy;
      $$invalidate(16, smActionBusy = rest);
    }
  };
  const smFixed = (serial, key, fallback = 0) => {
    var _a, _b;
    const cfg = keyerConfigForSerial(serial);
    return ((_b = (_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.fixed) == null ? void 0 : _b[key]) ?? fallback;
  };
  const smSequencerVal = (serial, phase, output, fallback = 0) => {
    var _a, _b, _c, _d;
    const cfg = keyerConfigForSerial(serial);
    return ((_d = (_c = (_b = (_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.fixed) == null ? void 0 : _b.sequencer) == null ? void 0 : _c[phase]) == null ? void 0 : _d[output]) ?? fallback;
  };
  const smOutputVal = (serial, output, fallback = 0) => {
    var _a, _b;
    const cfg = keyerConfigForSerial(serial);
    return ((_b = (_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.output) == null ? void 0 : _b[output]) ?? fallback;
  };
  const smSequencerOutputs = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    const outputs = ((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.output) || {};
    return smOutputNames.filter((name) => {
      const val = outputs[name];
      return val === 2 || val === 3;
    });
  };
  const applySmFixed = async (serial, key, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({
          serial,
          sm: { fixed: { [key]: Number(value) } }
        })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, "Setting updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update setting.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const applySmSequencer = async (serial, phase, output, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({
          serial,
          sm: {
            fixed: {
              sequencer: { [phase]: { [output]: Number(value) } }
            }
          }
        })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, "Sequencer setting updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update sequencer setting.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const applySmOutput = async (serial, output, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({
          serial,
          sm: { output: { [output]: Number(value) } }
        })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, "Output setting updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update output setting.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smAntennas = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    return (((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.obj) || []).filter((o) => o.type === 0);
  };
  const smAntOutputColumns = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    const outputs = ((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.output) || {};
    return smOutputNames.filter((name) => outputs[name] === 0);
  };
  const smAntNewForm = () => ({
    label: "",
    display: "",
    steppir: 0,
    rxonly: 0,
    pa_ant_number: 0,
    rotator: 0,
    rotator_offset: 0,
    output: {}
  });
  const smAntStartAdd = (serial) => {
    $$invalidate(17, smAntMode = { ...smAntMode, [serial]: "add" });
    $$invalidate(19, smAntAddForm = {
      ...smAntAddForm,
      [serial]: smAntNewForm()
    });
  };
  const smAntStartEdit = (serial) => {
    const ants = smAntennas(serial);
    const forms = {};
    for (const a of ants) {
      forms[a.id] = {
        label: a.label || "",
        display: a.display || "",
        steppir: a.steppir || 0,
        rxonly: a.rxonly || 0,
        pa_ant_number: a.pa_ant_number || 0,
        rotator: a.rotator || 0,
        rotator_offset: a.rotator_offset || 0,
        output: { ...a.output || {} }
      };
    }
    $$invalidate(18, smAntEditForms = { ...smAntEditForms, [serial]: forms });
    $$invalidate(17, smAntMode = { ...smAntMode, [serial]: "edit" });
  };
  const smAntCancel = (serial) => {
    $$invalidate(17, smAntMode = { ...smAntMode, [serial]: null });
  };
  const smAntSaveAdd = async (serial) => {
    const form = smAntAddForm[serial];
    if (!form) return;
    const payload = { action: "add", type: 0, ...form };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(17, smAntMode = { ...smAntMode, [serial]: null });
      setKeyerStatusMsg(serial, "Antenna added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add antenna.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smAntSaveEdit = async (serial) => {
    const forms = smAntEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const payload = {
          action: "mod",
          type: 0,
          id: Number(idStr),
          ...form
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(17, smAntMode = { ...smAntMode, [serial]: null });
      setKeyerStatusMsg(serial, "Antennas updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update antennas.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smAntRemoveSelected = async (serial) => {
    const sel = smAntSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      for (const id of sel) {
        const payload = { action: "rem", id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(20, smAntSelected = { ...smAntSelected, [serial]: [] });
      setKeyerStatusMsg(serial, "Antenna(s) removed.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to remove antenna.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smAntToggleSelect = (serial, id) => {
    const sel = smAntSelected[serial] || [];
    if (sel.includes(id)) {
      $$invalidate(20, smAntSelected = {
        ...smAntSelected,
        [serial]: sel.filter((x) => x !== id)
      });
    } else {
      $$invalidate(20, smAntSelected = { ...smAntSelected, [serial]: [...sel, id] });
    }
  };
  const smVirtualRotators = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    return (((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.obj) || []).filter((o) => o.type === 1 && o.virtual_rotator === 1);
  };
  const smAllObjects = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    return ((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.obj) || [];
  };
  const smObjDisplay = (serial, destId) => {
    const obj = smAllObjects(serial).find((o) => o.id === destId);
    return (obj == null ? void 0 : obj.display) || (obj == null ? void 0 : obj.label) || `#${destId}`;
  };
  const smObjRxOnly = (serial, destId) => {
    const obj = smAllObjects(serial).find((o) => o.id === destId);
    return (obj == null ? void 0 : obj.rxonly) ? "Yes" : "No";
  };
  const smVrStartAdd = (serial) => {
    $$invalidate(21, smVrMode = { ...smVrMode, [serial]: "add" });
    $$invalidate(22, smVrAddForm = {
      ...smVrAddForm,
      [serial]: { label: "", display: "" }
    });
    $$invalidate(25, smVrAddAnt = { ...smVrAddAnt, [serial]: null });
  };
  const smVrStartEdit = (serial) => {
    const vrs = smVirtualRotators(serial);
    const forms = {};
    for (const vr of vrs) {
      const refs = {};
      for (const ref of vr.refs || []) {
        refs[ref.id] = {
          min_azimuth: ref.min_azimuth || 0,
          max_azimuth: ref.max_azimuth || 0,
          dest_id: ref.dest_id || 0
        };
      }
      forms[vr.id] = {
        label: vr.label || "",
        display: vr.display || "",
        refs
      };
    }
    $$invalidate(23, smVrEditForms = { ...smVrEditForms, [serial]: forms });
    $$invalidate(21, smVrMode = { ...smVrMode, [serial]: "edit" });
    $$invalidate(25, smVrAddAnt = { ...smVrAddAnt, [serial]: null });
  };
  const smVrCancel = (serial) => {
    $$invalidate(21, smVrMode = { ...smVrMode, [serial]: null });
    $$invalidate(25, smVrAddAnt = { ...smVrAddAnt, [serial]: null });
  };
  const smVrSaveAdd = async (serial) => {
    const form = smVrAddForm[serial];
    if (!form) return;
    const payload = {
      action: "add",
      type: 1,
      virtual_rotator: 1,
      ...form
    };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(21, smVrMode = { ...smVrMode, [serial]: null });
      setKeyerStatusMsg(serial, "Virtual rotator added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add virtual rotator.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smVrSaveEdit = async (serial) => {
    const forms = smVrEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const refs = Object.entries(form.refs || {}).map(([rid, r]) => ({ id: Number(rid), ...r }));
        const payload = {
          action: "mod",
          type: 1,
          id: Number(idStr),
          label: form.label,
          display: form.display,
          refs
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(21, smVrMode = { ...smVrMode, [serial]: null });
      setKeyerStatusMsg(serial, "Virtual rotators updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update virtual rotators.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smVrRemoveSelected = async (serial) => {
    const sel = smVrSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      const refRemovals = sel.filter((s) => typeof s === "string" && s.includes(":"));
      const vrRemovals = sel.filter((s) => typeof s === "number");
      for (const key of refRemovals) {
        const [objId, refId] = key.split(":").map(Number);
        const payload = {
          action: "rem_ref",
          obj_id: objId,
          ref_id: refId
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      for (const id of vrRemovals) {
        const payload = { action: "rem", id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(24, smVrSelected = { ...smVrSelected, [serial]: [] });
      setKeyerStatusMsg(serial, "Item(s) removed.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to remove item(s).", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smVrToggleSelect = (serial, key) => {
    const sel = smVrSelected[serial] || [];
    const idx = sel.indexOf(key);
    if (idx >= 0) {
      $$invalidate(24, smVrSelected = {
        ...smVrSelected,
        [serial]: sel.filter((x) => x !== key)
      });
    } else {
      $$invalidate(24, smVrSelected = { ...smVrSelected, [serial]: [...sel, key] });
    }
  };
  const smVrStartAddAnt = (serial, vrId) => {
    $$invalidate(25, smVrAddAnt = {
      ...smVrAddAnt,
      [serial]: {
        vrId,
        min_azimuth: 0,
        max_azimuth: 0,
        dest_id: 0
      }
    });
  };
  const smVrSaveAddAnt = async (serial) => {
    const aa = smVrAddAnt[serial];
    if (!aa) return;
    const vr = smVirtualRotators(serial).find((v) => v.id === aa.vrId);
    if (!vr) return;
    const existingRefs = (vr.refs || []).map((r) => ({
      id: r.id,
      dest_id: r.dest_id,
      min_azimuth: r.min_azimuth,
      max_azimuth: r.max_azimuth
    }));
    const newRef = {
      id: 0,
      dest_id: aa.dest_id,
      min_azimuth: aa.min_azimuth,
      max_azimuth: aa.max_azimuth
    };
    const payload = {
      action: "mod",
      type: 1,
      id: aa.vrId,
      refs: [...existingRefs, newRef]
    };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(25, smVrAddAnt = { ...smVrAddAnt, [serial]: null });
      setKeyerStatusMsg(serial, "Antenna reference added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add antenna reference.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smVrCancelAddAnt = (serial) => {
    $$invalidate(25, smVrAddAnt = { ...smVrAddAnt, [serial]: null });
  };
  const smGroups = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    return (((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.obj) || []).filter((o) => o.type === 1 && !o.virtual_rotator);
  };
  const smGrpStartAdd = (serial) => {
    $$invalidate(26, smGrpMode = { ...smGrpMode, [serial]: "add" });
    $$invalidate(27, smGrpAddForm = {
      ...smGrpAddForm,
      [serial]: { label: "", display: "" }
    });
    $$invalidate(30, smGrpAddAnt = { ...smGrpAddAnt, [serial]: null });
  };
  const smGrpStartEdit = (serial) => {
    const grps = smGroups(serial);
    const forms = {};
    for (const grp of grps) {
      const refs = {};
      for (const ref of grp.refs || []) {
        refs[ref.id] = { dest_id: ref.dest_id || 0 };
      }
      forms[grp.id] = {
        label: grp.label || "",
        display: grp.display || "",
        refs
      };
    }
    $$invalidate(28, smGrpEditForms = { ...smGrpEditForms, [serial]: forms });
    $$invalidate(26, smGrpMode = { ...smGrpMode, [serial]: "edit" });
    $$invalidate(30, smGrpAddAnt = { ...smGrpAddAnt, [serial]: null });
  };
  const smGrpCancel = (serial) => {
    $$invalidate(26, smGrpMode = { ...smGrpMode, [serial]: null });
    $$invalidate(30, smGrpAddAnt = { ...smGrpAddAnt, [serial]: null });
  };
  const smGrpSaveAdd = async (serial) => {
    const form = smGrpAddForm[serial];
    if (!form) return;
    const payload = {
      action: "add",
      type: 1,
      virtual_rotator: 0,
      ...form
    };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(26, smGrpMode = { ...smGrpMode, [serial]: null });
      setKeyerStatusMsg(serial, "Antenna group added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add antenna group.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smGrpSaveEdit = async (serial) => {
    const forms = smGrpEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const refs = Object.entries(form.refs || {}).map(([rid, r]) => ({ id: Number(rid), ...r }));
        const payload = {
          action: "mod",
          type: 1,
          id: Number(idStr),
          label: form.label,
          display: form.display,
          refs
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(26, smGrpMode = { ...smGrpMode, [serial]: null });
      setKeyerStatusMsg(serial, "Antenna groups updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update antenna groups.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smGrpRemoveSelected = async (serial) => {
    const sel = smGrpSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      const refRemovals = sel.filter((s) => typeof s === "string" && s.includes(":"));
      const grpRemovals = sel.filter((s) => typeof s === "number");
      for (const key of refRemovals) {
        const [objId, refId] = key.split(":").map(Number);
        const payload = {
          action: "rem_ref",
          obj_id: objId,
          ref_id: refId
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      for (const id of grpRemovals) {
        const payload = { action: "rem", id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(29, smGrpSelected = { ...smGrpSelected, [serial]: [] });
      setKeyerStatusMsg(serial, "Item(s) removed.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to remove item(s).", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smGrpToggleSelect = (serial, key) => {
    const sel = smGrpSelected[serial] || [];
    const idx = sel.indexOf(key);
    if (idx >= 0) {
      $$invalidate(29, smGrpSelected = {
        ...smGrpSelected,
        [serial]: sel.filter((x) => x !== key)
      });
    } else {
      $$invalidate(29, smGrpSelected = {
        ...smGrpSelected,
        [serial]: [...sel, key]
      });
    }
  };
  const smGrpStartAddAnt = (serial, grpId) => {
    $$invalidate(30, smGrpAddAnt = {
      ...smGrpAddAnt,
      [serial]: { grpId, dest_id: 0 }
    });
  };
  const smGrpSaveAddAnt = async (serial) => {
    const aa = smGrpAddAnt[serial];
    if (!aa) return;
    const grp = smGroups(serial).find((g) => g.id === aa.grpId);
    if (!grp) return;
    const existingRefs = (grp.refs || []).map((r) => ({ id: r.id, dest_id: r.dest_id }));
    const newRef = { id: 0, dest_id: aa.dest_id };
    const payload = {
      action: "mod",
      type: 1,
      id: aa.grpId,
      refs: [...existingRefs, newRef]
    };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(30, smGrpAddAnt = { ...smGrpAddAnt, [serial]: null });
      setKeyerStatusMsg(serial, "Antenna reference added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add antenna reference.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smGrpCancelAddAnt = (serial) => {
    $$invalidate(30, smGrpAddAnt = { ...smGrpAddAnt, [serial]: null });
  };
  const smBands = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    return (((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.obj) || []).filter((o) => o.type === 2);
  };
  const smBpfOutputs = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    const outputs = ((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.output) || {};
    return smOutputNames.filter((name) => outputs[name] === 1);
  };
  const smAntOutputs = (serial) => {
    var _a;
    const cfg = keyerConfigForSerial(serial);
    const outputs = ((_a = cfg == null ? void 0 : cfg.sm) == null ? void 0 : _a.output) || {};
    return smOutputNames.filter((name) => outputs[name] === 0);
  };
  const smObjTypedDisplay = (serial, destId) => {
    const obj = smAllObjects(serial).find((o) => o.id === destId);
    if (!obj) return `#${destId}`;
    const name = obj.display || obj.label || `#${destId}`;
    if (obj.type === 0) return `ANT: ${name}`;
    if (obj.type === 1 && obj.virtual_rotator) return `VIR: ${name}`;
    if (obj.type === 1) return `GRP: ${name}`;
    return name;
  };
  const smAntsAndGroups = (serial) => {
    return smAllObjects(serial).filter((o) => o.type === 0 || o.type === 1);
  };
  const smBndStartAdd = (serial) => {
    $$invalidate(31, smBndMode = { ...smBndMode, [serial]: "add" });
    $$invalidate(32, smBndAddForm = {
      ...smBndAddForm,
      [serial]: {
        display: "",
        low_freq: 35e5,
        high_freq: 38e5,
        bcd_code: 0,
        pa_power: 0,
        keyout: 0
      }
    });
    $$invalidate(35, smBndAddRef = { ...smBndAddRef, [serial]: null });
  };
  const smBndStartEdit = (serial) => {
    const bnds = smBands(serial);
    const forms = {};
    for (const bnd of bnds) {
      const refs = {};
      for (const ref of bnd.refs || []) {
        refs[ref.id] = {
          dest_id: ref.dest_id || 0,
          rxonly: ref.rxonly || 0
        };
      }
      forms[bnd.id] = {
        display: bnd.display || "",
        low_freq: bnd.low_freq || 0,
        high_freq: bnd.high_freq || 0,
        bcd_code: bnd.bcd_code || 0,
        pa_power: bnd.pa_power || 0,
        keyout: bnd.keyout || 0,
        bpf_seq: { ...bnd.bpf_seq || {} },
        refs
      };
    }
    $$invalidate(33, smBndEditForms = { ...smBndEditForms, [serial]: forms });
    $$invalidate(31, smBndMode = { ...smBndMode, [serial]: "edit" });
    $$invalidate(35, smBndAddRef = { ...smBndAddRef, [serial]: null });
  };
  const smBndCancel = (serial) => {
    $$invalidate(31, smBndMode = { ...smBndMode, [serial]: null });
    $$invalidate(35, smBndAddRef = { ...smBndAddRef, [serial]: null });
  };
  const smBndSaveAdd = async (serial) => {
    const form = smBndAddForm[serial];
    if (!form) return;
    const payload = { action: "add", type: 2, ...form };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(31, smBndMode = { ...smBndMode, [serial]: null });
      setKeyerStatusMsg(serial, "Band added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add band.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smBndSaveEdit = async (serial) => {
    const forms = smBndEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const refs = Object.entries(form.refs || {}).map(([rid, r]) => ({ id: Number(rid), ...r }));
        const payload = {
          action: "mod",
          type: 2,
          id: Number(idStr),
          display: form.display,
          low_freq: form.low_freq,
          high_freq: form.high_freq,
          bcd_code: form.bcd_code,
          pa_power: form.pa_power,
          keyout: form.keyout,
          bpf_seq: form.bpf_seq,
          refs
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(31, smBndMode = { ...smBndMode, [serial]: null });
      setKeyerStatusMsg(serial, "Bands updated.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to update bands.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smBndRemoveSelected = async (serial) => {
    const sel = smBndSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      const refRemovals = sel.filter((s) => typeof s === "string" && s.includes(":"));
      const bndRemovals = sel.filter((s) => typeof s === "number");
      for (const key of refRemovals) {
        const [objId, refId] = key.split(":").map(Number);
        const payload = {
          action: "rem_ref",
          obj_id: objId,
          ref_id: refId
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      for (const id of bndRemovals) {
        const payload = { action: "rem", id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: "PATCH",
          headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
          },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      }
      $$invalidate(34, smBndSelected = { ...smBndSelected, [serial]: [] });
      setKeyerStatusMsg(serial, "Item(s) removed.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to remove item(s).", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smBndToggleSelect = (serial, key) => {
    const sel = smBndSelected[serial] || [];
    const idx = sel.indexOf(key);
    if (idx >= 0) {
      $$invalidate(34, smBndSelected = {
        ...smBndSelected,
        [serial]: sel.filter((x) => x !== key)
      });
    } else {
      $$invalidate(34, smBndSelected = {
        ...smBndSelected,
        [serial]: [...sel, key]
      });
    }
  };
  const smBndStartAddRef = (serial, bndId) => {
    $$invalidate(35, smBndAddRef = {
      ...smBndAddRef,
      [serial]: { bndId, dest_id: 0 }
    });
  };
  const smBndSaveAddRef = async (serial) => {
    const aa = smBndAddRef[serial];
    if (!aa) return;
    const bnd = smBands(serial).find((b) => b.id === aa.bndId);
    if (!bnd) return;
    const existingRefs = (bnd.refs || []).map((r) => ({
      id: r.id,
      dest_id: r.dest_id,
      rxonly: r.rxonly
    }));
    const newRef = { id: 0, dest_id: aa.dest_id };
    const payload = {
      action: "mod",
      type: 2,
      id: aa.bndId,
      refs: [...existingRefs, newRef]
    };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: "PATCH",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json"
        },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      $$invalidate(160, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
      $$invalidate(35, smBndAddRef = { ...smBndAddRef, [serial]: null });
      setKeyerStatusMsg(serial, "Antenna/group reference added.", "success");
    } catch (err) {
      setKeyerStatusMsg(serial, (err == null ? void 0 : err.message) || "Failed to add reference.", "error");
      await refreshDeviceConfig(serial);
    }
  };
  const smBndCancelAddRef = (serial) => {
    $$invalidate(35, smBndAddRef = { ...smBndAddRef, [serial]: null });
  };
  const activeKeyerMenuTitle = () => activeMenuId || "Keyer";
  const change_handler = (e) => updateKeyerParam(activeSerial, "r1FollowTxMode", e.target.checked ? 1 : 0);
  const change_handler_1 = (e) => updateKeyerParam(activeSerial, "r1KeyerMode", Number(e.target.value));
  const change_handler_2 = (e) => updateKeyerParam(activeSerial, "r2FollowTxMode", e.target.checked ? 1 : 0);
  const change_handler_3 = (e) => updateKeyerParam(activeSerial, "r2KeyerMode", Number(e.target.value));
  const change_handler_4 = (e) => updateRadioForm(activeSerial, "r1", "baud", Number(e.target.value));
  const change_handler_5 = (e) => updateRadioForm(activeSerial, "r1", "databits", Number(e.target.value));
  const change_handler_6 = (e) => updateRadioForm(activeSerial, "r1", "stopbits", Number(e.target.value));
  const change_handler_7 = (e) => updateRadioForm(activeSerial, "r1", "rtscts", e.target.checked ? 1 : 0);
  const change_handler_8 = (e) => setRigType(activeSerial, "r1", e.target.value);
  const input_handler = (e) => updateRadioForm(activeSerial, "r1", "icomaddress", Number(e.target.value));
  const change_handler_9 = (e) => updateRadioForm(activeSerial, "r1", "icomsimulateautoinfo", e.target.checked ? 1 : 0);
  const change_handler_10 = (e) => updateRadioForm(activeSerial, "r1", "digitalovervoicerule", Number(e.target.value));
  const change_handler_11 = (e) => updateRadioForm(activeSerial, "r1", "usedecoderifconnected", e.target.checked ? 1 : 0);
  const change_handler_12 = (e) => updateRadioForm(activeSerial, "r1", "dontinterfereusbcontrol", e.target.checked ? 1 : 0);
  const click_handler = () => applyRadioChannel(activeSerial, "r1");
  const change_handler_13 = (e) => updateRadioForm(activeSerial, "r2", "baud", Number(e.target.value));
  const change_handler_14 = (e) => updateRadioForm(activeSerial, "r2", "databits", Number(e.target.value));
  const change_handler_15 = (e) => updateRadioForm(activeSerial, "r2", "stopbits", Number(e.target.value));
  const change_handler_16 = (e) => updateRadioForm(activeSerial, "r2", "rtscts", e.target.checked ? 1 : 0);
  const change_handler_17 = (e) => setRigType(activeSerial, "r2", e.target.value);
  const input_handler_1 = (e) => updateRadioForm(activeSerial, "r2", "icomaddress", Number(e.target.value));
  const change_handler_18 = (e) => updateRadioForm(activeSerial, "r2", "icomsimulateautoinfo", e.target.checked ? 1 : 0);
  const change_handler_19 = (e) => updateRadioForm(activeSerial, "r2", "digitalovervoicerule", Number(e.target.value));
  const change_handler_20 = (e) => updateRadioForm(activeSerial, "r2", "usedecoderifconnected", e.target.checked ? 1 : 0);
  const change_handler_21 = (e) => updateRadioForm(activeSerial, "r2", "dontinterfereusbcontrol", e.target.checked ? 1 : 0);
  const click_handler_1 = () => applyRadioChannel(activeSerial, "r2");
  const change_handler_22 = (e) => updateRigModeSyncTop(activeSerial, "enabled", e.target.checked);
  const change_handler_23 = (e) => updateRigModeSyncTop(activeSerial, "backend", e.target.value);
  const change_handler_24 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "enabled", e.target.checked);
  const input_handler_2 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "host", e.target.value);
  const input_handler_3 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "port", Number(e.target.value));
  const input_handler_4 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "connect_timeout_ms", Number(e.target.value));
  const input_handler_5 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "io_timeout_ms", Number(e.target.value));
  const input_handler_6 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "poll_ms", Number(e.target.value));
  const change_handler_25 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "auto_start", e.target.checked);
  const input_handler_7 = (e) => updateRigModeSyncRadio(activeSerial, "r1", "rigctld_options", e.target.value);
  const change_handler_26 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "enabled", e.target.checked);
  const input_handler_8 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "host", e.target.value);
  const input_handler_9 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "port", Number(e.target.value));
  const input_handler_10 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "connect_timeout_ms", Number(e.target.value));
  const input_handler_11 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "io_timeout_ms", Number(e.target.value));
  const input_handler_12 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "poll_ms", Number(e.target.value));
  const change_handler_27 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "auto_start", e.target.checked);
  const input_handler_13 = (e) => updateRigModeSyncRadio(activeSerial, "r2", "rigctld_options", e.target.value);
  const click_handler_2 = () => applyRigModeSync(activeSerial);
  const change_handler_28 = (e) => updateRadioForm(activeSerial, "aux", "baud", Number(e.target.value));
  const change_handler_29 = (e) => updateRadioForm(activeSerial, "aux", "databits", Number(e.target.value));
  const change_handler_30 = (e) => updateRadioForm(activeSerial, "aux", "stopbits", Number(e.target.value));
  const change_handler_31 = (e) => updateRadioForm(activeSerial, "aux", "rtscts", e.target.checked ? 1 : 0);
  const click_handler_3 = () => applyRadioChannel(activeSerial, "aux");
  const change_handler_32 = (e) => updateRadioForm(activeSerial, "fsk1", "baud", Number(e.target.value));
  const change_handler_33 = (e) => updateRadioForm(activeSerial, "fsk1", "databits", Number(e.target.value));
  const change_handler_34 = (e) => updateRadioForm(activeSerial, "fsk1", "stopbits", Number(e.target.value));
  const change_handler_35 = (e) => updateRadioForm(activeSerial, "fsk1", "rtscts", e.target.checked ? 1 : 0);
  const change_handler_36 = (e) => updateKeyerParam(activeSerial, "r1InvertFsk", e.target.checked ? 1 : 0);
  const change_handler_37 = (e) => updateKeyerParam(activeSerial, "usePFsk", e.target.checked ? 1 : 0);
  const click_handler_4 = () => applyRadioChannel(activeSerial, "fsk1");
  const change_handler_38 = (e) => updateRadioForm(activeSerial, "fsk2", "baud", Number(e.target.value));
  const change_handler_39 = (e) => updateRadioForm(activeSerial, "fsk2", "databits", Number(e.target.value));
  const change_handler_40 = (e) => updateRadioForm(activeSerial, "fsk2", "stopbits", Number(e.target.value));
  const change_handler_41 = (e) => updateRadioForm(activeSerial, "fsk2", "rtscts", e.target.checked ? 1 : 0);
  const change_handler_42 = (e) => updateKeyerParam(activeSerial, "r2InvertFsk", e.target.checked ? 1 : 0);
  const change_handler_43 = (e) => updateKeyerParam(activeSerial, "usePFsk", e.target.checked ? 1 : 0);
  const click_handler_5 = () => applyRadioChannel(activeSerial, "fsk2");
  const change_handler_44 = (e) => applyPttChange(activeSerial, "r1", "cw", e.target.value);
  const change_handler_45 = (e) => applyPttChange(activeSerial, "r1", "voice", e.target.value);
  const change_handler_46 = (e) => applyPttChange(activeSerial, "r1", "digital", e.target.value);
  const change_handler_47 = (e) => updateKeyerParam(activeSerial, "r1LnaPtt", e.target.checked ? 1 : 0);
  const change_handler_48 = (e) => updateKeyerParam(activeSerial, "r1PaPtt", e.target.checked ? 1 : 0);
  const input_handler_14 = (e) => updateKeyerParam(activeSerial, "r1PaPttTail", Number(e.target.value));
  const input_handler_15 = (e) => updateKeyerParam(activeSerial, "r1LnaPttTail", Number(e.target.value));
  const input_handler_16 = (e) => updateKeyerParam(activeSerial, "r1PttDelay", Number(e.target.value));
  const change_handler_49 = (e) => updateKeyerParam(activeSerial, "r1AllowCwInVoice", e.target.checked ? 1 : 0);
  const change_handler_50 = (e) => updateKeyerParam(activeSerial, "useAutoPtt", e.target.checked ? 1 : 0);
  const change_handler_51 = (e) => updateKeyerParam(activeSerial, "downstreamOverFootSw", e.target.checked ? 1 : 0);
  const change_handler_52 = (e) => applyPttChange(activeSerial, "r2", "cw", e.target.value);
  const change_handler_53 = (e) => applyPttChange(activeSerial, "r2", "voice", e.target.value);
  const change_handler_54 = (e) => applyPttChange(activeSerial, "r2", "digital", e.target.value);
  const change_handler_55 = (e) => updateKeyerParam(activeSerial, "r2LnaPtt", e.target.checked ? 1 : 0);
  const change_handler_56 = (e) => updateKeyerParam(activeSerial, "r2PaPtt", e.target.checked ? 1 : 0);
  const input_handler_17 = (e) => updateKeyerParam(activeSerial, "r2PaPttTail", Number(e.target.value));
  const input_handler_18 = (e) => updateKeyerParam(activeSerial, "r2LnaPttTail", Number(e.target.value));
  const input_handler_19 = (e) => updateKeyerParam(activeSerial, "r2PttDelay", Number(e.target.value));
  const change_handler_57 = (e) => updateKeyerParam(activeSerial, "r2AllowCwInVoice", e.target.checked ? 1 : 0);
  const change_handler_58 = (e) => updateKeyerParam(activeSerial, "muteCompCw", e.target.checked ? 1 : 0);
  const change_handler_59 = (e) => updateKeyerParam(activeSerial, "muteCompFsk", e.target.checked ? 1 : 0);
  const change_handler_60 = (e) => updateKeyerParam(activeSerial, "restorePtt", e.target.checked ? 1 : 0);
  const change_handler_61 = (e) => updateKeyerParam(activeSerial, "restoreCompCw", e.target.checked ? 1 : 0);
  const change_handler_62 = (e) => updateKeyerParam(activeSerial, "restoreCompFsk", e.target.checked ? 1 : 0);
  const change_handler_63 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRx`,
    Number(e.target.value)
  );
  const change_handler_64 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTx`,
    Number(e.target.value)
  );
  const change_handler_65 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSw`,
    Number(e.target.value)
  );
  const change_handler_66 = (e) => updateKeyerParam(activeSerial, "r1FrMpkExtra_Voice.onAirRecActive", e.target.checked ? 1 : 0);
  const change_handler_67 = (e) => updateKeyerParam(activeSerial, "r1FrMpkExtra_Digital.onAirRecActive", e.target.checked ? 1 : 0);
  const change_handler_68 = (e) => updateKeyerParam(activeSerial, "r1FrMpkExtra_Voice.onAirRecControlByRouter", e.target.checked ? 1 : 0);
  const change_handler_69 = (e) => updateKeyerParam(activeSerial, "r1FrMpkExtra_Digital.onAirRecControlByRouter", e.target.checked ? 1 : 0);
  const input_handler_20 = (e) => updateKeyerParam(activeSerial, "r1FrMpkExtra_Voice.pwmMonctr", Number(e.target.value));
  const input_handler_21 = (e) => updateKeyerParam(activeSerial, "r1FrMpkExtra_Digital.pwmMonctr", Number(e.target.value));
  const change_handler_70 = (e) => updateKeyerParams(activeSerial, {
    micSelAuto: e.target.value === "auto" ? 1 : 0,
    micSelFront: e.target.value === "front" ? 1 : 0
  });
  const change_handler_71 = (e) => updateKeyerParam(activeSerial, "useAutoPtt", e.target.checked ? 1 : 0);
  const change_handler_72 = (e) => updateKeyerParam(activeSerial, "downstreamOverFootSw", e.target.checked ? 1 : 0);
  const change_handler_73 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRx`,
    Number(e.target.value)
  );
  const change_handler_74 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRxMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_75 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTx`,
    Number(e.target.value)
  );
  const change_handler_76 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_77 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSw`,
    Number(e.target.value)
  );
  const change_handler_78 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSwMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_79 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.voiceCodec`,
    Number(e.target.value)
  );
  const change_handler_80 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRx`,
    Number(e.target.value)
  );
  const change_handler_81 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRxMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_82 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTx`,
    Number(e.target.value)
  );
  const change_handler_83 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_84 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSw`,
    Number(e.target.value)
  );
  const change_handler_85 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSwMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_86 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r1FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.voiceCodec`,
    Number(e.target.value)
  );
  const change_handler_87 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRx`,
    Number(e.target.value)
  );
  const change_handler_88 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioRxMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_89 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTx`,
    Number(e.target.value)
  );
  const change_handler_90 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_91 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrBase_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSw`,
    Number(e.target.value)
  );
  const change_handler_92 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.audioTxFootSwMic`,
    e.target.checked ? 1 : 0
  );
  const change_handler_93 = (mode, e) => updateKeyerParam(
    activeSerial,
    `r2FrMokExtra_${mode === "cw" ? "Cw" : mode === "voice" ? "Voice" : "Digital"}.voiceCodec`,
    Number(e.target.value)
  );
  const input_handler_22 = (e) => updateKeyerParam(activeSerial, "pwmBlight", Number(e.target.value));
  const input_handler_23 = (e) => updateKeyerParam(activeSerial, "pwmContrast", Number(e.target.value));
  const change_handler_94 = (e) => updateKeyerParam(activeSerial, "dispBg0", Number(e.target.value));
  const change_handler_95 = (e) => updateKeyerParam(activeSerial, "dispBg1", Number(e.target.value));
  const change_handler_96 = (opt, e) => updateKeyerParam(activeSerial, `dispEv.${opt.value}`, e.target.checked ? 1 : 0);
  const change_handler_97 = (opt, e) => updateKeyerParam(activeSerial, `dispEvLn.${opt.value}`, Number(e.target.value));
  const change_handler_98 = (e) => updateKeyerParam(activeSerial, "paddleOnlySideTone", e.target.checked ? 1 : 0);
  const change_handler_99 = (e) => updateWinkeyParam(activeSerial, "keyMode", Number(e.target.value));
  const change_handler_100 = (e) => updateWinkeyParam(activeSerial, "ultimaticMode", Number(e.target.value));
  const input_handler_24 = (e) => updateWinkeyParam(activeSerial, "leadInTime", Number(e.target.value));
  const input_handler_25 = (e) => updateWinkeyParam(activeSerial, "tailTime", Number(e.target.value));
  const input_handler_26 = (e) => updateWinkeyParam(activeSerial, "hangTime", Number(e.target.value));
  const change_handler_101 = (e) => updateWinkeyParam(activeSerial, "paddleSwap", e.target.checked ? 1 : 0);
  const change_handler_102 = (e) => updateWinkeyParam(activeSerial, "autoSpace", e.target.checked ? 1 : 0);
  const change_handler_103 = (e) => updateWinkeyParam(activeSerial, "ctSpacing", e.target.checked ? 1 : 0);
  const change_handler_104 = (e) => updateWinkeyParam(activeSerial, "disablePaddleWatchdog", e.target.checked ? 1 : 0);
  const input_handler_27 = (e) => updateWinkeyParam(activeSerial, "minWpm", Number(e.target.value));
  const input_handler_28 = (e) => updateWinkeyParam(activeSerial, "wpmRange", Number(e.target.value));
  const input_handler_29 = (e) => updateWinkeyParam(activeSerial, "farnsWpm", Number(e.target.value));
  const input_handler_30 = (e) => updateWinkeyParam(activeSerial, "ditDahRatio", Number(e.target.value));
  const input_handler_31 = (e) => updateWinkeyParam(activeSerial, "weight", Number(e.target.value));
  const input_handler_32 = (e) => updateWinkeyParam(activeSerial, "1stExtension", Number(e.target.value));
  const input_handler_33 = (e) => updateWinkeyParam(activeSerial, "keyComp", Number(e.target.value));
  const input_handler_34 = (e) => updateWinkeyParam(activeSerial, "paddleSetpoint", Number(e.target.value));
  const change_handler_105 = (e) => updateKeyerParam(activeSerial, "sideTone", Number(e.target.value));
  const change_handler_106 = (e) => updateKeyerParam(activeSerial, "paddleOnlySideTone", e.target.checked ? 1 : 0);
  const input_handler_35 = (idx, e) => handleMessageInput(activeSerial, "cw", idx, e.target);
  const click_handler_6 = (idx) => playMessage(activeSerial, idx);
  const click_handler_7 = () => applyMessages(activeSerial, "cw");
  const input_handler_36 = (idx, e) => handleMessageInput(activeSerial, "fsk", idx, e.target);
  const click_handler_8 = (idx) => playMessage(activeSerial, idx);
  const click_handler_9 = () => applyMessages(activeSerial, "fsk");
  const click_handler_10 = () => smAction(activeSerial, "sm_load");
  const click_handler_11 = () => smAction(activeSerial, "sm_store");
  const change_handler_107 = (e) => applySmFixed(activeSerial, "civFunc", e.target.value);
  const change_handler_108 = (e) => applySmFixed(activeSerial, "civBaudRate", e.target.value);
  const change_handler_109 = (e) => applySmFixed(activeSerial, "civAddress", e.target.value);
  const change_handler_110 = (e) => applySmFixed(activeSerial, "extSerFunc", e.target.value);
  const change_handler_111 = (e) => applySmFixed(activeSerial, "extSerBaudRate", e.target.value);
  const change_handler_112 = (e) => applySmFixed(activeSerial, "antSwDelay", e.target.value);
  const change_handler_113 = (e) => applySmFixed(activeSerial, "bbmDelay", e.target.value);
  const change_handler_114 = (e) => applySmFixed(activeSerial, "inhibitLead", e.target.value);
  const change_handler_115 = (e) => applySmFixed(activeSerial, "useKeyIn", e.target.checked ? 1 : 0);
  const change_handler_116 = (e) => applySmFixed(activeSerial, "invertKeyIn", e.target.checked ? 1 : 0);
  const change_handler_117 = (output, e) => applySmSequencer(activeSerial, "lead", output, e.target.value);
  const change_handler_118 = (output, e) => applySmSequencer(activeSerial, "tail", output, e.target.value);
  const change_handler_119 = (output, e) => applySmOutput(activeSerial, output, e.target.value);
  const change_handler_120 = (ant) => smAntToggleSelect(activeSerial, ant.id);
  const input_handler_37 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].label = e.target.value, smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const input_handler_38 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].display = e.target.value, smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const change_handler_121 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].steppir = Number(e.target.value), smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const change_handler_122 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].rxonly = Number(e.target.value), smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const input_handler_39 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].pa_ant_number = Number(e.target.value), smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const change_handler_123 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].rotator = Number(e.target.value), smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const input_handler_40 = (ant, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].rotator_offset = Number(e.target.value), smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  const change_handler_124 = (ant, oc, e) => {
    $$invalidate(18, smAntEditForms[activeSerial][ant.id].output[oc] = Number(e.target.value), smAntEditForms);
    $$invalidate(18, smAntEditForms);
  };
  function input0_input_handler() {
    smAntAddForm[activeSerial].label = this.value;
    $$invalidate(19, smAntAddForm);
  }
  function input1_input_handler() {
    smAntAddForm[activeSerial].display = this.value;
    $$invalidate(19, smAntAddForm);
  }
  function select0_change_handler() {
    smAntAddForm[activeSerial].steppir = select_value(this);
    $$invalidate(19, smAntAddForm);
  }
  function select1_change_handler() {
    smAntAddForm[activeSerial].rxonly = select_value(this);
    $$invalidate(19, smAntAddForm);
  }
  function input2_input_handler() {
    smAntAddForm[activeSerial].pa_ant_number = to_number(this.value);
    $$invalidate(19, smAntAddForm);
  }
  function select2_change_handler() {
    smAntAddForm[activeSerial].rotator = select_value(this);
    $$invalidate(19, smAntAddForm);
  }
  function input3_input_handler() {
    smAntAddForm[activeSerial].rotator_offset = to_number(this.value);
    $$invalidate(19, smAntAddForm);
  }
  function select_change_handler(oc) {
    smAntAddForm[activeSerial].output[oc] = select_value(this);
    $$invalidate(19, smAntAddForm);
  }
  const click_handler_12 = () => smAntSaveAdd(activeSerial);
  const click_handler_13 = () => smAntCancel(activeSerial);
  const click_handler_14 = () => smAntSaveEdit(activeSerial);
  const click_handler_15 = () => smAntCancel(activeSerial);
  const click_handler_16 = () => smAntStartAdd(activeSerial);
  const click_handler_17 = () => smAntStartEdit(activeSerial);
  const click_handler_18 = () => smAntRemoveSelected(activeSerial);
  const change_handler_125 = (vr) => smVrToggleSelect(activeSerial, vr.id);
  const input_handler_41 = (vr, e) => {
    $$invalidate(23, smVrEditForms[activeSerial][vr.id].label = e.target.value, smVrEditForms);
    $$invalidate(23, smVrEditForms);
  };
  const input_handler_42 = (vr, e) => {
    $$invalidate(23, smVrEditForms[activeSerial][vr.id].display = e.target.value, smVrEditForms);
    $$invalidate(23, smVrEditForms);
  };
  const click_handler_19 = (vr) => smVrStartAddAnt(activeSerial, vr.id);
  const change_handler_126 = (vr, ref) => smVrToggleSelect(activeSerial, `${vr.id}:${ref.id}`);
  const input_handler_43 = (vr, ref, e) => {
    $$invalidate(23, smVrEditForms[activeSerial][vr.id].refs[ref.id].min_azimuth = Number(e.target.value), smVrEditForms);
    $$invalidate(23, smVrEditForms);
  };
  const input_handler_44 = (vr, ref, e) => {
    $$invalidate(23, smVrEditForms[activeSerial][vr.id].refs[ref.id].max_azimuth = Number(e.target.value), smVrEditForms);
    $$invalidate(23, smVrEditForms);
  };
  const change_handler_127 = (vr, ref, e) => {
    $$invalidate(23, smVrEditForms[activeSerial][vr.id].refs[ref.id].dest_id = Number(e.target.value), smVrEditForms);
    $$invalidate(23, smVrEditForms);
  };
  function input0_input_handler_1() {
    smVrAddAnt[activeSerial].min_azimuth = to_number(this.value);
    $$invalidate(25, smVrAddAnt);
    $$invalidate(49, activeAllObjs), $$invalidate(160, configDevices), $$invalidate(0, activeSerial);
  }
  function input1_input_handler_1() {
    smVrAddAnt[activeSerial].max_azimuth = to_number(this.value);
    $$invalidate(25, smVrAddAnt);
    $$invalidate(49, activeAllObjs), $$invalidate(160, configDevices), $$invalidate(0, activeSerial);
  }
  function select_change_handler_1() {
    smVrAddAnt[activeSerial].dest_id = select_value(this);
    $$invalidate(25, smVrAddAnt);
    $$invalidate(49, activeAllObjs), $$invalidate(160, configDevices), $$invalidate(0, activeSerial);
  }
  function input0_input_handler_2() {
    smVrAddForm[activeSerial].label = this.value;
    $$invalidate(22, smVrAddForm);
  }
  function input1_input_handler_2() {
    smVrAddForm[activeSerial].display = this.value;
    $$invalidate(22, smVrAddForm);
  }
  const click_handler_20 = () => smVrSaveAddAnt(activeSerial);
  const click_handler_21 = () => smVrCancelAddAnt(activeSerial);
  const click_handler_22 = () => smVrSaveAdd(activeSerial);
  const click_handler_23 = () => smVrCancel(activeSerial);
  const click_handler_24 = () => smVrSaveEdit(activeSerial);
  const click_handler_25 = () => smVrCancel(activeSerial);
  const click_handler_26 = () => smVrStartAdd(activeSerial);
  const click_handler_27 = () => smVrStartEdit(activeSerial);
  const click_handler_28 = () => smVrRemoveSelected(activeSerial);
  const change_handler_128 = (grp) => smGrpToggleSelect(activeSerial, grp.id);
  const input_handler_45 = (grp, e) => {
    $$invalidate(28, smGrpEditForms[activeSerial][grp.id].label = e.target.value, smGrpEditForms);
    $$invalidate(28, smGrpEditForms);
  };
  const input_handler_46 = (grp, e) => {
    $$invalidate(28, smGrpEditForms[activeSerial][grp.id].display = e.target.value, smGrpEditForms);
    $$invalidate(28, smGrpEditForms);
  };
  const click_handler_29 = (grp) => smGrpStartAddAnt(activeSerial, grp.id);
  const change_handler_129 = (grp, ref) => smGrpToggleSelect(activeSerial, `${grp.id}:${ref.id}`);
  const change_handler_130 = (grp, ref, e) => {
    $$invalidate(28, smGrpEditForms[activeSerial][grp.id].refs[ref.id].dest_id = Number(e.target.value), smGrpEditForms);
    $$invalidate(28, smGrpEditForms);
  };
  function select_change_handler_2() {
    smGrpAddAnt[activeSerial].dest_id = select_value(this);
    $$invalidate(30, smGrpAddAnt);
    $$invalidate(49, activeAllObjs), $$invalidate(160, configDevices), $$invalidate(0, activeSerial);
  }
  function input0_input_handler_3() {
    smGrpAddForm[activeSerial].label = this.value;
    $$invalidate(27, smGrpAddForm);
  }
  function input1_input_handler_3() {
    smGrpAddForm[activeSerial].display = this.value;
    $$invalidate(27, smGrpAddForm);
  }
  const click_handler_30 = () => smGrpSaveAddAnt(activeSerial);
  const click_handler_31 = () => smGrpCancelAddAnt(activeSerial);
  const click_handler_32 = () => smGrpSaveAdd(activeSerial);
  const click_handler_33 = () => smGrpCancel(activeSerial);
  const click_handler_34 = () => smGrpSaveEdit(activeSerial);
  const click_handler_35 = () => smGrpCancel(activeSerial);
  const click_handler_36 = () => smGrpStartAdd(activeSerial);
  const click_handler_37 = () => smGrpStartEdit(activeSerial);
  const click_handler_38 = () => smGrpRemoveSelected(activeSerial);
  const change_handler_131 = (bnd) => smBndToggleSelect(activeSerial, bnd.id);
  const input_handler_47 = (bnd, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].display = e.target.value, smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const input_handler_48 = (bnd, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].low_freq = Number(e.target.value), smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const input_handler_49 = (bnd, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].high_freq = Number(e.target.value), smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const input_handler_50 = (bnd, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].bcd_code = Number(e.target.value), smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const change_handler_132 = (bnd, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].pa_power = e.target.checked ? 1 : 0, smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const change_handler_133 = (bnd, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].keyout = e.target.checked ? 1 : 0, smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const change_handler_134 = (bnd, bo, e) => {
    if (!smBndEditForms[activeSerial][bnd.id].bpf_seq) $$invalidate(33, smBndEditForms[activeSerial][bnd.id].bpf_seq = {}, smBndEditForms);
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].bpf_seq[bo] = e.target.checked ? 1 : 0, smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const change_handler_135 = (bnd, so, e) => {
    if (!smBndEditForms[activeSerial][bnd.id].bpf_seq) $$invalidate(33, smBndEditForms[activeSerial][bnd.id].bpf_seq = {}, smBndEditForms);
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].bpf_seq[so] = e.target.checked ? 1 : 0, smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const change_handler_136 = (bnd, ref) => smBndToggleSelect(activeSerial, `${bnd.id}:${ref.id}`);
  const change_handler_137 = (bnd, ref, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].refs[ref.id].dest_id = Number(e.target.value), smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const change_handler_138 = (bnd, ref, e) => {
    $$invalidate(33, smBndEditForms[activeSerial][bnd.id].refs[ref.id].rxonly = e.target.checked ? 1 : 0, smBndEditForms);
    $$invalidate(33, smBndEditForms);
  };
  const func = (ref, o) => o.id === ref.dest_id;
  function select_change_handler_3() {
    smBndAddRef[activeSerial].dest_id = select_value(this);
    $$invalidate(35, smBndAddRef);
    $$invalidate(44, activeAntsAndGroups), $$invalidate(160, configDevices), $$invalidate(0, activeSerial);
  }
  const click_handler_39 = () => smBndSaveAddRef(activeSerial);
  const click_handler_40 = () => smBndCancelAddRef(activeSerial);
  const click_handler_41 = (bnd) => smBndStartAddRef(activeSerial, bnd.id);
  function input0_input_handler_4() {
    smBndAddForm[activeSerial].display = this.value;
    $$invalidate(32, smBndAddForm);
  }
  function input1_input_handler_4() {
    smBndAddForm[activeSerial].low_freq = to_number(this.value);
    $$invalidate(32, smBndAddForm);
  }
  function input2_input_handler_1() {
    smBndAddForm[activeSerial].high_freq = to_number(this.value);
    $$invalidate(32, smBndAddForm);
  }
  function input3_input_handler_1() {
    smBndAddForm[activeSerial].bcd_code = to_number(this.value);
    $$invalidate(32, smBndAddForm);
  }
  function input4_change_handler() {
    smBndAddForm[activeSerial].pa_power = this.checked;
    $$invalidate(32, smBndAddForm);
  }
  function input5_change_handler() {
    smBndAddForm[activeSerial].keyout = this.checked;
    $$invalidate(32, smBndAddForm);
  }
  const click_handler_42 = () => smBndSaveAdd(activeSerial);
  const click_handler_43 = () => smBndCancel(activeSerial);
  const click_handler_44 = () => smBndSaveEdit(activeSerial);
  const click_handler_45 = () => smBndCancel(activeSerial);
  const click_handler_46 = () => smBndStartAdd(activeSerial);
  const click_handler_47 = () => smBndStartEdit(activeSerial);
  const click_handler_48 = () => smBndRemoveSelected(activeSerial);
  const input_handler_51 = (entry, e) => updateAllParamForm(activeSerial, entry[0], Number(e.target.value));
  const click_handler_49 = () => applyAllParams(activeSerial);
  $$self.$$set = ($$props2) => {
    if ("activeSerial" in $$props2) $$invalidate(0, activeSerial = $$props2.activeSerial);
    if ("activeMenuId" in $$props2) $$invalidate(1, activeMenuId = $$props2.activeMenuId);
    if ("activeKeyer" in $$props2) $$invalidate(2, activeKeyer = $$props2.activeKeyer);
    if ("activeKeyerFlags" in $$props2) $$invalidate(161, activeKeyerFlags = $$props2.activeKeyerFlags);
    if ("configDevices" in $$props2) $$invalidate(160, configDevices = $$props2.configDevices);
    if ("metadata" in $$props2) $$invalidate(162, metadata = $$props2.metadata);
    if ("keyers" in $$props2) $$invalidate(163, keyers = $$props2.keyers);
    if ("refreshDeviceConfig" in $$props2) $$invalidate(164, refreshDeviceConfig = $$props2.refreshDeviceConfig);
  };
  $$self.$$.update = () => {
    var _a, _b, _c, _d, _e, _f, _g;
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(63, hasKeyerMode = activeKeyerFlags.includes("HAS_KEYER_MODE"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(3, hasR2 = activeKeyerFlags.includes("HAS_R2"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(6, hasAux = activeKeyerFlags.includes("HAS_AUX"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(5, hasFsk1 = activeKeyerFlags.includes("HAS_FSK1"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(4, hasFsk2 = activeKeyerFlags.includes("HAS_FSK2"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(62, hasPfsk = activeKeyerFlags.includes("HAS_PFSK"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(61, hasFollowTx = activeKeyerFlags.includes("HAS_FOLLOW_TX_MODE"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(60, hasLnaPaPtt = activeKeyerFlags.includes("HAS_LNA_PA_PTT"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(59, hasLnaPaPttTail = activeKeyerFlags.includes("HAS_LNA_PA_PTT_TAIL"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(58, hasSoundcardPtt = activeKeyerFlags.includes("HAS_SOUNDCARD_PTT"));
    }
    if ($$self.$$.dirty[5] & /*activeKeyerFlags*/
    64) {
      $$invalidate(57, hasCwInVoice = activeKeyerFlags.includes("HAS_CW_IN_VOICE"));
    }
    if ($$self.$$.dirty[0] & /*activeKeyer*/
    4) {
      $$invalidate(56, isAudioMk1 = (activeKeyer == null ? void 0 : activeKeyer.type) === 4);
    }
    if ($$self.$$.dirty[0] & /*activeKeyer*/
    4) {
      $$invalidate(55, isAudioMk2 = (activeKeyer == null ? void 0 : activeKeyer.type) === 5 || (activeKeyer == null ? void 0 : activeKeyer.type) === 11);
    }
    if ($$self.$$.dirty[0] & /*activeKeyer*/
    4) {
      $$invalidate(54, isAudioMk2R = (activeKeyer == null ? void 0 : activeKeyer.type) === 6 || (activeKeyer == null ? void 0 : activeKeyer.type) === 7);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(53, activeAnts = configDevices && activeSerial ? smAntennas(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(52, activeAntOutCols = configDevices && activeSerial ? smAntOutputColumns(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(51, activeVrs = configDevices && activeSerial ? smVirtualRotators(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(50, activeGrps = configDevices && activeSerial ? smGroups(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(49, activeAllObjs = configDevices && activeSerial ? smAllObjects(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(48, activeBnds = configDevices && activeSerial ? smBands(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(47, activeBpfOutputs = configDevices && activeSerial ? smBpfOutputs(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(46, activeSeqOutputs = configDevices && activeSerial ? smSequencerOutputs(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(45, activeAntOutputs = configDevices && activeSerial ? smAntOutputs(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*configDevices*/
    32) {
      $$invalidate(44, activeAntsAndGroups = configDevices && activeSerial ? smAntsAndGroups(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*radioForm*/
    1024) {
      $$invalidate(43, activeR1 = activeSerial ? ((_a = radioForm[activeSerial]) == null ? void 0 : _a.r1) || defaultRadioForm(activeSerial, "r1") : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*radioForm*/
    1024) {
      $$invalidate(42, activeR2 = activeSerial ? ((_b = radioForm[activeSerial]) == null ? void 0 : _b.r2) || defaultRadioForm(activeSerial, "r2") : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*radioForm*/
    1024) {
      $$invalidate(41, activeAux = activeSerial ? ((_c = radioForm[activeSerial]) == null ? void 0 : _c.aux) || defaultRadioForm(activeSerial, "aux") : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*radioForm*/
    1024) {
      $$invalidate(40, activeFsk1 = activeSerial ? ((_d = radioForm[activeSerial]) == null ? void 0 : _d.fsk1) || defaultRadioForm(activeSerial, "fsk1") : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*radioForm*/
    1024) {
      $$invalidate(39, activeFsk2 = activeSerial ? ((_e = radioForm[activeSerial]) == null ? void 0 : _e.fsk2) || defaultRadioForm(activeSerial, "fsk2") : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*rigModeSyncForm*/
    4096) {
      $$invalidate(38, activeRigModeSync = activeSerial ? rigModeSyncForm[activeSerial] || defaultRigModeSyncForm(activeSerial) : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*pttForm*/
    8192) {
      $$invalidate(37, activePttR1 = activeSerial ? ((_f = pttForm[activeSerial]) == null ? void 0 : _f.r1) || defaultPttForm(activeSerial, "r1") : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial*/
    1 | $$self.$$.dirty[5] & /*pttForm*/
    8192) {
      $$invalidate(36, activePttR2 = activeSerial ? ((_g = pttForm[activeSerial]) == null ? void 0 : _g.r2) || defaultPttForm(activeSerial, "r2") : null);
    }
    if ($$self.$$.dirty[5] & /*configDevices*/
    32) {
      if (configDevices) {
        resetRadioForms();
        resetRigModeSyncForms();
        resetPttForms();
        resetMessageForms();
        resetAllParamForms();
      }
    }
    if ($$self.$$.dirty[0] & /*activeSerial, hasR2, hasAux, hasFsk1, hasFsk2*/
    121 | $$self.$$.dirty[5] & /*configDevices, radioFormGen, pttFormGen, messageFormGen, allParamFormGen*/
    116768) {
      if (activeSerial && configDevices) {
        ensureRadioForm(activeSerial, "r1");
        if (hasR2) ensureRadioForm(activeSerial, "r2");
        if (hasAux) ensureRadioForm(activeSerial, "aux");
        if (hasFsk1) ensureRadioForm(activeSerial, "fsk1");
        if (hasFsk2) ensureRadioForm(activeSerial, "fsk2");
        ensureRigModeSyncForm(activeSerial);
        ensurePttForm(activeSerial, "r1");
        if (hasR2) ensurePttForm(activeSerial, "r2");
        ensureMessageForm(activeSerial, "cw");
        ensureMessageForm(activeSerial, "fsk");
        ensureAllParamForm(activeSerial);
      }
    }
  };
  return [
    activeSerial,
    activeMenuId,
    activeKeyer,
    hasR2,
    hasFsk2,
    hasFsk1,
    hasAux,
    keyerStatus,
    radioStatus,
    rigModeSyncStatus,
    pttStatus,
    messageForm,
    messageStatus,
    allParamForm,
    allParamStatus,
    smActionStatus,
    smActionBusy,
    smAntMode,
    smAntEditForms,
    smAntAddForm,
    smAntSelected,
    smVrMode,
    smVrAddForm,
    smVrEditForms,
    smVrSelected,
    smVrAddAnt,
    smGrpMode,
    smGrpAddForm,
    smGrpEditForms,
    smGrpSelected,
    smGrpAddAnt,
    smBndMode,
    smBndAddForm,
    smBndEditForms,
    smBndSelected,
    smBndAddRef,
    activePttR2,
    activePttR1,
    activeRigModeSync,
    activeFsk2,
    activeFsk1,
    activeAux,
    activeR2,
    activeR1,
    activeAntsAndGroups,
    activeAntOutputs,
    activeSeqOutputs,
    activeBpfOutputs,
    activeBnds,
    activeAllObjs,
    activeGrps,
    activeVrs,
    activeAntOutCols,
    activeAnts,
    isAudioMk2R,
    isAudioMk2,
    isAudioMk1,
    hasCwInVoice,
    hasSoundcardPtt,
    hasLnaPaPttTail,
    hasLnaPaPtt,
    hasFollowTx,
    hasPfsk,
    hasKeyerMode,
    playMessage,
    keyerModeOptions,
    radioBaudOptions,
    radioDataBitsOptions,
    radioStopBitsOptions,
    fskBaudOptions,
    fskDataBitsOptions,
    fskStopBitsOptions,
    mk2MicSelOptions,
    mk2rCodecOptions,
    winkeyModeOptions,
    winkeyUltimaticOptions,
    sideToneOptions,
    displayLineOptions,
    messageSlots,
    pttOptionLabels,
    digitalOverVoiceOptions,
    smCivFuncOptions,
    smCivBaudOptions,
    smExtSerFuncOptions,
    smOutputClassOptions,
    smOutputNames,
    hasFlag,
    updateRadioForm,
    updateRigModeSyncTop,
    updateRigModeSyncRadio,
    applyRigModeSync,
    handleMessageInput,
    updateAllParamForm,
    applyAllParams,
    applyMessages,
    audioOptionsFor,
    audioOptionLabel,
    mk2MicSelValue,
    displayBackgroundOptions,
    displayEventOptions,
    pttOptionsFor,
    applyPttChange,
    applyRadioChannel,
    rigtypes,
    setRigType,
    keyerParam,
    updateKeyerParam,
    updateWinkeyParam,
    winkeyValue,
    updateKeyerParams,
    smAction,
    smFixed,
    smSequencerVal,
    smOutputVal,
    smSequencerOutputs,
    applySmFixed,
    applySmSequencer,
    applySmOutput,
    smAntNewForm,
    smAntStartAdd,
    smAntStartEdit,
    smAntCancel,
    smAntSaveAdd,
    smAntSaveEdit,
    smAntRemoveSelected,
    smAntToggleSelect,
    smObjDisplay,
    smObjRxOnly,
    smVrStartAdd,
    smVrStartEdit,
    smVrCancel,
    smVrSaveAdd,
    smVrSaveEdit,
    smVrRemoveSelected,
    smVrToggleSelect,
    smVrStartAddAnt,
    smVrSaveAddAnt,
    smVrCancelAddAnt,
    smGrpStartAdd,
    smGrpStartEdit,
    smGrpCancel,
    smGrpSaveAdd,
    smGrpSaveEdit,
    smGrpRemoveSelected,
    smGrpToggleSelect,
    smGrpStartAddAnt,
    smGrpSaveAddAnt,
    smGrpCancelAddAnt,
    smObjTypedDisplay,
    smBndStartAdd,
    smBndStartEdit,
    smBndCancel,
    smBndSaveAdd,
    smBndSaveEdit,
    smBndRemoveSelected,
    smBndToggleSelect,
    smBndStartAddRef,
    smBndSaveAddRef,
    smBndCancelAddRef,
    activeKeyerMenuTitle,
    configDevices,
    activeKeyerFlags,
    metadata,
    keyers,
    refreshDeviceConfig,
    radioForm,
    radioFormGen,
    rigModeSyncForm,
    pttForm,
    pttFormGen,
    messageFormGen,
    allParamFormGen,
    change_handler,
    change_handler_1,
    change_handler_2,
    change_handler_3,
    change_handler_4,
    change_handler_5,
    change_handler_6,
    change_handler_7,
    change_handler_8,
    input_handler,
    change_handler_9,
    change_handler_10,
    change_handler_11,
    change_handler_12,
    click_handler,
    change_handler_13,
    change_handler_14,
    change_handler_15,
    change_handler_16,
    change_handler_17,
    input_handler_1,
    change_handler_18,
    change_handler_19,
    change_handler_20,
    change_handler_21,
    click_handler_1,
    change_handler_22,
    change_handler_23,
    change_handler_24,
    input_handler_2,
    input_handler_3,
    input_handler_4,
    input_handler_5,
    input_handler_6,
    change_handler_25,
    input_handler_7,
    change_handler_26,
    input_handler_8,
    input_handler_9,
    input_handler_10,
    input_handler_11,
    input_handler_12,
    change_handler_27,
    input_handler_13,
    click_handler_2,
    change_handler_28,
    change_handler_29,
    change_handler_30,
    change_handler_31,
    click_handler_3,
    change_handler_32,
    change_handler_33,
    change_handler_34,
    change_handler_35,
    change_handler_36,
    change_handler_37,
    click_handler_4,
    change_handler_38,
    change_handler_39,
    change_handler_40,
    change_handler_41,
    change_handler_42,
    change_handler_43,
    click_handler_5,
    change_handler_44,
    change_handler_45,
    change_handler_46,
    change_handler_47,
    change_handler_48,
    input_handler_14,
    input_handler_15,
    input_handler_16,
    change_handler_49,
    change_handler_50,
    change_handler_51,
    change_handler_52,
    change_handler_53,
    change_handler_54,
    change_handler_55,
    change_handler_56,
    input_handler_17,
    input_handler_18,
    input_handler_19,
    change_handler_57,
    change_handler_58,
    change_handler_59,
    change_handler_60,
    change_handler_61,
    change_handler_62,
    change_handler_63,
    change_handler_64,
    change_handler_65,
    change_handler_66,
    change_handler_67,
    change_handler_68,
    change_handler_69,
    input_handler_20,
    input_handler_21,
    change_handler_70,
    change_handler_71,
    change_handler_72,
    change_handler_73,
    change_handler_74,
    change_handler_75,
    change_handler_76,
    change_handler_77,
    change_handler_78,
    change_handler_79,
    change_handler_80,
    change_handler_81,
    change_handler_82,
    change_handler_83,
    change_handler_84,
    change_handler_85,
    change_handler_86,
    change_handler_87,
    change_handler_88,
    change_handler_89,
    change_handler_90,
    change_handler_91,
    change_handler_92,
    change_handler_93,
    input_handler_22,
    input_handler_23,
    change_handler_94,
    change_handler_95,
    change_handler_96,
    change_handler_97,
    change_handler_98,
    change_handler_99,
    change_handler_100,
    input_handler_24,
    input_handler_25,
    input_handler_26,
    change_handler_101,
    change_handler_102,
    change_handler_103,
    change_handler_104,
    input_handler_27,
    input_handler_28,
    input_handler_29,
    input_handler_30,
    input_handler_31,
    input_handler_32,
    input_handler_33,
    input_handler_34,
    change_handler_105,
    change_handler_106,
    input_handler_35,
    click_handler_6,
    click_handler_7,
    input_handler_36,
    click_handler_8,
    click_handler_9,
    click_handler_10,
    click_handler_11,
    change_handler_107,
    change_handler_108,
    change_handler_109,
    change_handler_110,
    change_handler_111,
    change_handler_112,
    change_handler_113,
    change_handler_114,
    change_handler_115,
    change_handler_116,
    change_handler_117,
    change_handler_118,
    change_handler_119,
    change_handler_120,
    input_handler_37,
    input_handler_38,
    change_handler_121,
    change_handler_122,
    input_handler_39,
    change_handler_123,
    input_handler_40,
    change_handler_124,
    input0_input_handler,
    input1_input_handler,
    select0_change_handler,
    select1_change_handler,
    input2_input_handler,
    select2_change_handler,
    input3_input_handler,
    select_change_handler,
    click_handler_12,
    click_handler_13,
    click_handler_14,
    click_handler_15,
    click_handler_16,
    click_handler_17,
    click_handler_18,
    change_handler_125,
    input_handler_41,
    input_handler_42,
    click_handler_19,
    change_handler_126,
    input_handler_43,
    input_handler_44,
    change_handler_127,
    input0_input_handler_1,
    input1_input_handler_1,
    select_change_handler_1,
    input0_input_handler_2,
    input1_input_handler_2,
    click_handler_20,
    click_handler_21,
    click_handler_22,
    click_handler_23,
    click_handler_24,
    click_handler_25,
    click_handler_26,
    click_handler_27,
    click_handler_28,
    change_handler_128,
    input_handler_45,
    input_handler_46,
    click_handler_29,
    change_handler_129,
    change_handler_130,
    select_change_handler_2,
    input0_input_handler_3,
    input1_input_handler_3,
    click_handler_30,
    click_handler_31,
    click_handler_32,
    click_handler_33,
    click_handler_34,
    click_handler_35,
    click_handler_36,
    click_handler_37,
    click_handler_38,
    change_handler_131,
    input_handler_47,
    input_handler_48,
    input_handler_49,
    input_handler_50,
    change_handler_132,
    change_handler_133,
    change_handler_134,
    change_handler_135,
    change_handler_136,
    change_handler_137,
    change_handler_138,
    func,
    select_change_handler_3,
    click_handler_39,
    click_handler_40,
    click_handler_41,
    input0_input_handler_4,
    input1_input_handler_4,
    input2_input_handler_1,
    input3_input_handler_1,
    input4_change_handler,
    input5_change_handler,
    click_handler_42,
    click_handler_43,
    click_handler_44,
    click_handler_45,
    click_handler_46,
    click_handler_47,
    click_handler_48,
    input_handler_51,
    click_handler_49
  ];
}
class KeyerPage extends SvelteComponent {
  constructor(options) {
    super();
    init(
      this,
      options,
      instance$1,
      create_fragment$1,
      safe_not_equal,
      {
        activeSerial: 0,
        activeMenuId: 1,
        activeKeyer: 2,
        activeKeyerFlags: 161,
        configDevices: 160,
        metadata: 162,
        keyers: 163,
        refreshDeviceConfig: 164
      },
      null,
      [
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1
      ]
    );
  }
}
function create_else_block(ctx) {
  let section;
  return {
    c() {
      section = element("section");
      section.innerHTML = `<div class="section-title">Keyer</div> <div class="panel"><div class="placeholder">Keyer pages will be implemented next.</div></div>`;
      attr(section, "class", "section");
    },
    m(target, anchor) {
      insert(target, section, anchor);
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(section);
      }
    }
  };
}
function create_if_block_5(ctx) {
  let keyerpage;
  let updating_configDevices;
  let current;
  function keyerpage_configDevices_binding(value) {
    ctx[31](value);
  }
  let keyerpage_props = {
    activeSerial: (
      /*activeSerial*/
      ctx[7]
    ),
    activeMenuId: (
      /*activeMenuId*/
      ctx[9]
    ),
    activeKeyer: (
      /*activeKeyer*/
      ctx[8]
    ),
    activeKeyerFlags: (
      /*activeKeyerFlags*/
      ctx[17]
    ),
    metadata: (
      /*metadata*/
      ctx[2]
    ),
    keyers: (
      /*keyers*/
      ctx[3]
    ),
    refreshDeviceConfig: (
      /*refreshDeviceConfig*/
      ctx[20]
    )
  };
  if (
    /*configDevices*/
    ctx[1] !== void 0
  ) {
    keyerpage_props.configDevices = /*configDevices*/
    ctx[1];
  }
  keyerpage = new KeyerPage({ props: keyerpage_props });
  binding_callbacks.push(() => bind(keyerpage, "configDevices", keyerpage_configDevices_binding));
  return {
    c() {
      create_component(keyerpage.$$.fragment);
    },
    m(target, anchor) {
      mount_component(keyerpage, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const keyerpage_changes = {};
      if (dirty[0] & /*activeSerial*/
      128) keyerpage_changes.activeSerial = /*activeSerial*/
      ctx2[7];
      if (dirty[0] & /*activeMenuId*/
      512) keyerpage_changes.activeMenuId = /*activeMenuId*/
      ctx2[9];
      if (dirty[0] & /*activeKeyer*/
      256) keyerpage_changes.activeKeyer = /*activeKeyer*/
      ctx2[8];
      if (dirty[0] & /*activeKeyerFlags*/
      131072) keyerpage_changes.activeKeyerFlags = /*activeKeyerFlags*/
      ctx2[17];
      if (dirty[0] & /*metadata*/
      4) keyerpage_changes.metadata = /*metadata*/
      ctx2[2];
      if (dirty[0] & /*keyers*/
      8) keyerpage_changes.keyers = /*keyers*/
      ctx2[3];
      if (!updating_configDevices && dirty[0] & /*configDevices*/
      2) {
        updating_configDevices = true;
        keyerpage_changes.configDevices = /*configDevices*/
        ctx2[1];
        add_flush_callback(() => updating_configDevices = false);
      }
      keyerpage.$set(keyerpage_changes);
    },
    i(local) {
      if (current) return;
      transition_in(keyerpage.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(keyerpage.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(keyerpage, detaching);
    }
  };
}
function create_if_block_4(ctx) {
  let daemonsettings;
  let updating_daemonCfg;
  let current;
  function daemonsettings_daemonCfg_binding(value) {
    ctx[30](value);
  }
  let daemonsettings_props = {};
  if (
    /*daemonCfg*/
    ctx[11] !== void 0
  ) {
    daemonsettings_props.daemonCfg = /*daemonCfg*/
    ctx[11];
  }
  daemonsettings = new DaemonSettings({ props: daemonsettings_props });
  binding_callbacks.push(() => bind(daemonsettings, "daemonCfg", daemonsettings_daemonCfg_binding));
  return {
    c() {
      create_component(daemonsettings.$$.fragment);
    },
    m(target, anchor) {
      mount_component(daemonsettings, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const daemonsettings_changes = {};
      if (!updating_daemonCfg && dirty[0] & /*daemonCfg*/
      2048) {
        updating_daemonCfg = true;
        daemonsettings_changes.daemonCfg = /*daemonCfg*/
        ctx2[11];
        add_flush_callback(() => updating_daemonCfg = false);
      }
      daemonsettings.$set(daemonsettings_changes);
    },
    i(local) {
      if (current) return;
      transition_in(daemonsettings.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(daemonsettings.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(daemonsettings, detaching);
    }
  };
}
function create_if_block_3(ctx) {
  let daemonports;
  let current;
  daemonports = new DaemonPorts({
    props: {
      connectors: (
        /*connectors*/
        ctx[12]
      ),
      keyers: (
        /*keyers*/
        ctx[3]
      ),
      devices: (
        /*devices*/
        ctx[0]
      ),
      metadata: (
        /*metadata*/
        ctx[2]
      ),
      reloadData: (
        /*reloadData*/
        ctx[22]
      )
    }
  });
  return {
    c() {
      create_component(daemonports.$$.fragment);
    },
    m(target, anchor) {
      mount_component(daemonports, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const daemonports_changes = {};
      if (dirty[0] & /*connectors*/
      4096) daemonports_changes.connectors = /*connectors*/
      ctx2[12];
      if (dirty[0] & /*keyers*/
      8) daemonports_changes.keyers = /*keyers*/
      ctx2[3];
      if (dirty[0] & /*devices*/
      1) daemonports_changes.devices = /*devices*/
      ctx2[0];
      if (dirty[0] & /*metadata*/
      4) daemonports_changes.metadata = /*metadata*/
      ctx2[2];
      daemonports.$set(daemonports_changes);
    },
    i(local) {
      if (current) return;
      transition_in(daemonports.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(daemonports.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(daemonports, detaching);
    }
  };
}
function create_if_block_2(ctx) {
  let homesummary;
  let current;
  homesummary = new HomeSummary({
    props: {
      runtime: (
        /*runtime*/
        ctx[10]
      ),
      daemonCfg: (
        /*daemonCfg*/
        ctx[11]
      ),
      devices: (
        /*devices*/
        ctx[0]
      ),
      fwString: (
        /*fwString*/
        ctx[21]
      ),
      reloadData: (
        /*reloadData*/
        ctx[22]
      )
    }
  });
  return {
    c() {
      create_component(homesummary.$$.fragment);
    },
    m(target, anchor) {
      mount_component(homesummary, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const homesummary_changes = {};
      if (dirty[0] & /*runtime*/
      1024) homesummary_changes.runtime = /*runtime*/
      ctx2[10];
      if (dirty[0] & /*daemonCfg*/
      2048) homesummary_changes.daemonCfg = /*daemonCfg*/
      ctx2[11];
      if (dirty[0] & /*devices*/
      1) homesummary_changes.devices = /*devices*/
      ctx2[0];
      homesummary.$set(homesummary_changes);
    },
    i(local) {
      if (current) return;
      transition_in(homesummary.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(homesummary.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(homesummary, detaching);
    }
  };
}
function create_if_block_1(ctx) {
  let div;
  let t;
  return {
    c() {
      div = element("div");
      t = text(
        /*error*/
        ctx[13]
      );
      attr(div, "class", "error");
    },
    m(target, anchor) {
      insert(target, div, anchor);
      append(div, t);
    },
    p(ctx2, dirty) {
      if (dirty[0] & /*error*/
      8192) set_data(
        t,
        /*error*/
        ctx2[13]
      );
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_if_block(ctx) {
  let div;
  return {
    c() {
      div = element("div");
      div.textContent = "Loading…";
      attr(div, "class", "loading");
    },
    m(target, anchor) {
      insert(target, div, anchor);
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
function create_fragment(ctx) {
  var _a, _b, _c;
  let div2;
  let connectionoverlay;
  let t0;
  let topbar;
  let t1;
  let tabnav;
  let t2;
  let div1;
  let sidemenu;
  let t3;
  let main;
  let div0;
  let t4;
  let t5;
  let h1;
  let t6;
  let t7;
  let show_if;
  let current_block_type_index;
  let if_block;
  let current;
  connectionoverlay = new ConnectionOverlay({});
  topbar = new TopBar({
    props: {
      hostname: (
        /*runtime*/
        ((_a = ctx[10]) == null ? void 0 : _a.hostname) || "—"
      ),
      version: (
        /*runtime*/
        ((_c = (_b = ctx[10]) == null ? void 0 : _b.daemon) == null ? void 0 : _c.version) || "—"
      )
    }
  });
  tabnav = new TabNav({
    props: {
      tabs: (
        /*tabs*/
        ctx[18]
      ),
      activeTab: (
        /*activeTab*/
        ctx[5]
      )
    }
  });
  tabnav.$on(
    "select",
    /*select_handler*/
    ctx[28]
  );
  sidemenu = new SideMenu({
    props: {
      items: (
        /*sideMenuItems*/
        ctx[14]
      ),
      activeId: (
        /*activeMenuId*/
        ctx[9]
      )
    }
  });
  sidemenu.$on(
    "select",
    /*select_handler_1*/
    ctx[29]
  );
  const if_block_creators = [
    create_if_block,
    create_if_block_1,
    create_if_block_2,
    create_if_block_3,
    create_if_block_4,
    create_if_block_5,
    create_else_block
  ];
  const if_blocks = [];
  function select_block_type(ctx2, dirty) {
    if (dirty[0] & /*activeTab*/
    32) show_if = null;
    if (
      /*loading*/
      ctx2[4]
    ) return 0;
    if (
      /*error*/
      ctx2[13]
    ) return 1;
    if (
      /*activeTab*/
      ctx2[5] === "daemon" && /*activeMenu*/
      ctx2[6] === "summary"
    ) return 2;
    if (
      /*activeTab*/
      ctx2[5] === "daemon" && /*activeMenu*/
      ctx2[6] === "ports"
    ) return 3;
    if (
      /*activeTab*/
      ctx2[5] === "daemon" && /*activeMenu*/
      ctx2[6] === "settings"
    ) return 4;
    if (show_if == null) show_if = !!/*activeTab*/
    ctx2[5].startsWith("keyer:");
    if (show_if) return 5;
    return 6;
  }
  current_block_type_index = select_block_type(ctx, [-1, -1]);
  if_block = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx);
  return {
    c() {
      div2 = element("div");
      create_component(connectionoverlay.$$.fragment);
      t0 = space();
      create_component(topbar.$$.fragment);
      t1 = space();
      create_component(tabnav.$$.fragment);
      t2 = space();
      div1 = element("div");
      create_component(sidemenu.$$.fragment);
      t3 = space();
      main = element("main");
      div0 = element("div");
      t4 = text(
        /*breadcrumb*/
        ctx[15]
      );
      t5 = space();
      h1 = element("h1");
      t6 = text(
        /*activePageTitle*/
        ctx[16]
      );
      t7 = space();
      if_block.c();
      attr(div0, "class", "breadcrumbs");
      attr(main, "class", "main");
      attr(div1, "class", "layout");
      attr(div2, "class", "page");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      mount_component(connectionoverlay, div2, null);
      append(div2, t0);
      mount_component(topbar, div2, null);
      append(div2, t1);
      mount_component(tabnav, div2, null);
      append(div2, t2);
      append(div2, div1);
      mount_component(sidemenu, div1, null);
      append(div1, t3);
      append(div1, main);
      append(main, div0);
      append(div0, t4);
      append(main, t5);
      append(main, h1);
      append(h1, t6);
      append(main, t7);
      if_blocks[current_block_type_index].m(main, null);
      current = true;
    },
    p(ctx2, dirty) {
      var _a2, _b2, _c2;
      const topbar_changes = {};
      if (dirty[0] & /*runtime*/
      1024) topbar_changes.hostname = /*runtime*/
      ((_a2 = ctx2[10]) == null ? void 0 : _a2.hostname) || "—";
      if (dirty[0] & /*runtime*/
      1024) topbar_changes.version = /*runtime*/
      ((_c2 = (_b2 = ctx2[10]) == null ? void 0 : _b2.daemon) == null ? void 0 : _c2.version) || "—";
      topbar.$set(topbar_changes);
      const tabnav_changes = {};
      if (dirty[0] & /*tabs*/
      262144) tabnav_changes.tabs = /*tabs*/
      ctx2[18];
      if (dirty[0] & /*activeTab*/
      32) tabnav_changes.activeTab = /*activeTab*/
      ctx2[5];
      tabnav.$set(tabnav_changes);
      const sidemenu_changes = {};
      if (dirty[0] & /*sideMenuItems*/
      16384) sidemenu_changes.items = /*sideMenuItems*/
      ctx2[14];
      if (dirty[0] & /*activeMenuId*/
      512) sidemenu_changes.activeId = /*activeMenuId*/
      ctx2[9];
      sidemenu.$set(sidemenu_changes);
      if (!current || dirty[0] & /*breadcrumb*/
      32768) set_data(
        t4,
        /*breadcrumb*/
        ctx2[15]
      );
      if (!current || dirty[0] & /*activePageTitle*/
      65536) set_data(
        t6,
        /*activePageTitle*/
        ctx2[16]
      );
      let previous_block_index = current_block_type_index;
      current_block_type_index = select_block_type(ctx2, dirty);
      if (current_block_type_index === previous_block_index) {
        if_blocks[current_block_type_index].p(ctx2, dirty);
      } else {
        group_outros();
        transition_out(if_blocks[previous_block_index], 1, 1, () => {
          if_blocks[previous_block_index] = null;
        });
        check_outros();
        if_block = if_blocks[current_block_type_index];
        if (!if_block) {
          if_block = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx2);
          if_block.c();
        } else {
          if_block.p(ctx2, dirty);
        }
        transition_in(if_block, 1);
        if_block.m(main, null);
      }
    },
    i(local) {
      if (current) return;
      transition_in(connectionoverlay.$$.fragment, local);
      transition_in(topbar.$$.fragment, local);
      transition_in(tabnav.$$.fragment, local);
      transition_in(sidemenu.$$.fragment, local);
      transition_in(if_block);
      current = true;
    },
    o(local) {
      transition_out(connectionoverlay.$$.fragment, local);
      transition_out(topbar.$$.fragment, local);
      transition_out(tabnav.$$.fragment, local);
      transition_out(sidemenu.$$.fragment, local);
      transition_out(if_block);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      destroy_component(connectionoverlay);
      destroy_component(topbar);
      destroy_component(tabnav);
      destroy_component(sidemenu);
      if_blocks[current_block_type_index].d();
    }
  };
}
function instance($$self, $$props, $$invalidate) {
  let tabs;
  let activeSerial;
  let activeKeyer;
  let activeKeyerFlags;
  let activeKeyerMenus;
  let activeMenuId;
  let activeMenuMeta;
  let activeTabLabel;
  let activePageTitle;
  let breadcrumb;
  let sideMenuItems;
  let runtime = null;
  let daemonCfg = null;
  let devices = [];
  let configDevices = [];
  let connectors = [];
  let metadata = null;
  let keyers = [];
  let loading = true;
  let error = "";
  let lastSerial = "";
  const parseHash = () => {
    const h = location.hash.replace(/^#\/?/, "");
    const [t, m] = h.split("/");
    return { tab: t || "daemon", menu: m || "" };
  };
  const initHash = parseHash();
  let activeTab = initHash.tab;
  let activeMenu = initHash.menu || "summary";
  let hashRestoredMenu = !!initHash.menu;
  const baseTabs = [{ id: "daemon", label: "Daemon" }];
  const daemonMenus = [
    {
      id: "summary",
      label: "Summary",
      title: "Summary"
    },
    {
      id: "ports",
      label: "Ports",
      title: "Daemon Ports Configuration and Routing"
    },
    {
      id: "settings",
      label: "Settings",
      title: "Daemon Settings"
    },
    {
      id: "action",
      label: "Action",
      title: "Daemon Actions",
      disabled: true
    }
  ];
  const keyerMenus = [
    {
      id: "mode",
      label: "Keyer Mode",
      title: "Keyer Mode",
      depends: "has.keyer_mode"
    },
    {
      id: "radio",
      label: "Radio",
      title: "Radio Communication Parameters",
      depends: "has.r1"
    },
    {
      id: "aux",
      label: "AUX",
      title: "AUX Communication Parameters",
      depends: "has.aux"
    },
    {
      id: "fsk",
      label: "FSK",
      title: "FSK Channel Parameters",
      depends: "has.fsk1"
    },
    {
      id: "ptt",
      label: "PTT",
      title: "PTT Configuration",
      depends: "has.ptt_settings"
    },
    {
      id: "audio",
      label: "Audio",
      title: "Audio Configuration",
      depends: "has.audio_switching"
    },
    {
      id: "display",
      label: "Display",
      title: "Display Configuration",
      depends: "has.display"
    },
    {
      id: "cw",
      label: "CW",
      title: "CW Settings",
      depends: "has.winkey"
    },
    {
      id: "cw_messages",
      label: "CW/FSK Messages",
      title: "Messages",
      depends: "has.winkey"
    },
    {
      id: "sm_antsw_load_store",
      label: "Switching / Load, Store",
      title: "Antenna Switching / Load & Store",
      depends: "has.sm_commands"
    },
    {
      id: "sm_antsw_settings",
      label: "Switching / Settings",
      title: "Antenna Switching / Settings",
      depends: "has.sm_commands"
    },
    {
      id: "sm_antsw_outputs",
      label: "Switching / Outputs",
      title: "Antenna Switching / Outputs",
      depends: "has.sm_commands"
    },
    {
      id: "sm_antsw_ant_list",
      label: "Switching / Antennas",
      title: "Antenna Switching / Antennas",
      depends: "has.sm_commands"
    },
    {
      id: "sm_antsw_vr_list",
      label: "Switching / Virt. Rotators",
      title: "Antenna Switching / Virtual Rotators",
      depends: "has.sm_commands"
    },
    {
      id: "sm_antsw_grp_list",
      label: "Switching / Ant. Groups",
      title: "Antenna Switching / Antenna Groups",
      depends: "has.sm_commands"
    },
    {
      id: "sm_antsw_band_list",
      label: "Switching / Bands",
      title: "Antenna Switching / Bands",
      depends: "has.sm_commands"
    },
    {
      id: "all_params",
      label: "All Parameters",
      title: "All Keyer Parameters"
    }
  ];
  const dependsMap = {
    "has.keyer_mode": "HAS_KEYER_MODE",
    "has.r1": "HAS_R1",
    "has.aux": "HAS_AUX",
    "has.fsk1": "HAS_FSK1",
    "has.ptt_settings": "HAS_PTT_SETTINGS",
    "has.audio_switching": "HAS_AUDIO_SWITCHING",
    "has.display": "HAS_DISPLAY",
    "has.winkey": "HAS_WINKEY",
    "has.sm_commands": "HAS_SM_COMMANDS"
  };
  const updateHash = () => {
    const h = `#${activeTab}/${activeMenu}`;
    if (location.hash !== h) history.replaceState(null, "", h);
  };
  const setTab = (id) => {
    $$invalidate(5, activeTab = id);
    if (id === "daemon") $$invalidate(6, activeMenu = "summary");
    else if (id.startsWith("keyer:")) $$invalidate(6, activeMenu = "mode");
    else $$invalidate(6, activeMenu = "summary");
  };
  const refreshDeviceConfig = async (serial) => {
    try {
      const updated = await apiGet(`/api/v1/config/devices/${serial}`);
      $$invalidate(1, configDevices = configDevices.map((d) => d.serial === serial ? updated : d));
    } catch {
    }
  };
  const fwString = (d) => {
    if (!d) return "";
    const major = d.verFwMajor ?? 0;
    const minor = d.verFwMinor ?? 0;
    const beta = d.verFwBeta ? "b" : "";
    return `${major}.${minor}${beta}`;
  };
  const reloadData = async () => {
    try {
      const data = await loadAllData();
      $$invalidate(10, runtime = data.runtime);
      $$invalidate(11, daemonCfg = data.daemonCfg);
      $$invalidate(0, devices = data.devices);
      $$invalidate(1, configDevices = data.configDevices);
      $$invalidate(12, connectors = data.connectors);
      $$invalidate(2, metadata = data.metadata);
    } catch (err) {
      console.error("Failed to reload data:", err);
      throw err;
    }
  };
  const unsubStatus = onWsEvent("status", (data) => {
    const known = devices.find((d) => d.serial === data.serial);
    if (!known) {
      reloadData().catch(() => {
      });
      return;
    }
    const fwUnknown = !known.verFwMajor && !known.verFwMinor;
    if (data.status === "ONLINE" && fwUnknown) {
      reloadData().catch(() => {
      });
      return;
    }
    $$invalidate(0, devices = devices.map((d) => d.serial === data.serial ? { ...d, status: data.status } : d));
  });
  const unsubRemoved = onWsEvent("device_removed", (data) => {
    $$invalidate(0, devices = devices.filter((d) => d.serial !== data.serial));
    $$invalidate(1, configDevices = configDevices.filter((d) => d.serial !== data.serial));
    $$invalidate(12, connectors = connectors.filter((c) => c.serial !== data.serial));
  });
  const unsubUsb = onWsEvent("usb_connection", (data) => {
    console.log("USB Event:", data);
  });
  onMount(async () => {
    try {
      await reloadData();
      connectWs(async () => {
        try {
          await reloadData();
        } catch {
        }
      });
    } catch (err) {
      $$invalidate(13, error = (err == null ? void 0 : err.message) || "Failed to load data");
      connectWs(async () => {
        try {
          await reloadData();
        } catch {
        }
      });
    } finally {
      $$invalidate(4, loading = false);
    }
    const onHashChange = () => {
      const h = parseHash();
      if (h.tab !== activeTab) $$invalidate(5, activeTab = h.tab);
      if (h.menu && h.menu !== activeMenu) $$invalidate(6, activeMenu = h.menu);
    };
    window.addEventListener("hashchange", onHashChange);
    return () => {
      window.removeEventListener("hashchange", onHashChange);
    };
  });
  onDestroy(() => {
    disconnectWs();
    unsubStatus();
    unsubRemoved();
    unsubUsb();
  });
  const buildKeyers = (cfgDevices, devs) => {
    const bySerial = new Map((devs || []).map((d) => [d.serial, d]));
    if (cfgDevices && cfgDevices.length) {
      return cfgDevices.map((cfg) => {
        const dev = bySerial.get(cfg.serial) || {};
        return {
          serial: cfg.serial,
          type: cfg.type,
          name: dev.name || cfg.serial,
          status: dev.status || ""
        };
      });
    }
    return (devs || []).map((dev) => ({
      serial: dev.serial,
      type: null,
      name: dev.name || dev.serial,
      status: dev.status || ""
    }));
  };
  const keyerFlagsForSerial = (serial) => {
    const cfg = keyers.find((k) => k.serial === serial);
    if (!cfg || cfg.type == null || !(metadata == null ? void 0 : metadata.devicetypes)) return [];
    const devType = metadata.devicetypes.find((d) => d.type === cfg.type);
    return (devType == null ? void 0 : devType.flags) || [];
  };
  const visibleKeyerMenus = (serial) => {
    const flags = keyerFlagsForSerial(serial);
    return keyerMenus.filter((m) => {
      if (!m.depends) return true;
      const flag = dependsMap[m.depends];
      return flag ? flags.includes(flag) : false;
    });
  };
  const select_handler = (e) => setTab(e.detail);
  const select_handler_1 = (e) => $$invalidate(6, activeMenu = e.detail);
  function daemonsettings_daemonCfg_binding(value) {
    daemonCfg = value;
    $$invalidate(11, daemonCfg);
  }
  function keyerpage_configDevices_binding(value) {
    configDevices = value;
    $$invalidate(1, configDevices);
  }
  $$self.$$.update = () => {
    var _a;
    if ($$self.$$.dirty[0] & /*configDevices, devices*/
    3) {
      $$invalidate(3, keyers = buildKeyers(configDevices, devices));
    }
    if ($$self.$$.dirty[0] & /*keyers*/
    8) {
      $$invalidate(18, tabs = [
        ...baseTabs,
        ...keyers.map((k) => ({
          id: `keyer:${k.serial}`,
          label: k.name,
          serial: k.serial,
          status: k.status
        }))
      ]);
    }
    if ($$self.$$.dirty[0] & /*activeTab*/
    32) {
      $$invalidate(7, activeSerial = activeTab.startsWith("keyer:") ? activeTab.split(":")[1] : "");
    }
    if ($$self.$$.dirty[0] & /*activeSerial, keyers*/
    136) {
      $$invalidate(8, activeKeyer = activeSerial ? keyers.find((k) => k.serial === activeSerial) : null);
    }
    if ($$self.$$.dirty[0] & /*activeSerial, keyers, metadata*/
    140) {
      $$invalidate(17, activeKeyerFlags = activeSerial && keyers && metadata ? keyerFlagsForSerial(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*activeSerial, keyers, metadata*/
    140) {
      $$invalidate(25, activeKeyerMenus = activeSerial && keyers && metadata ? visibleKeyerMenus(activeSerial) : []);
    }
    if ($$self.$$.dirty[0] & /*loading, activeSerial, activeKeyer*/
    400) {
      if (!loading && activeSerial && !activeKeyer) setTab("daemon");
    }
    if ($$self.$$.dirty[0] & /*activeTab, activeSerial, lastSerial, hashRestoredMenu, activeKeyerMenus*/
    58720416) {
      if (activeTab.startsWith("keyer:") && activeSerial && activeSerial !== lastSerial) {
        $$invalidate(23, lastSerial = activeSerial);
        if (hashRestoredMenu) {
          $$invalidate(24, hashRestoredMenu = false);
        } else {
          $$invalidate(6, activeMenu = ((_a = activeKeyerMenus[0]) == null ? void 0 : _a.id) || "summary");
        }
      }
    }
    if ($$self.$$.dirty[0] & /*activeMenu*/
    64) {
      $$invalidate(9, activeMenuId = (activeMenu || "").toLowerCase());
    }
    if ($$self.$$.dirty[0] & /*activeTab, activeMenu*/
    96) {
      updateHash();
    }
    if ($$self.$$.dirty[0] & /*activeTab, activeMenuId, activeKeyerMenus*/
    33554976) {
      $$invalidate(27, activeMenuMeta = activeTab === "daemon" ? daemonMenus.find((m) => m.id === activeMenuId) || null : activeTab.startsWith("keyer:") ? activeKeyerMenus.find((m) => m.id === activeMenuId) || null : null);
    }
    if ($$self.$$.dirty[0] & /*activeTab, activeKeyer, activeSerial*/
    416) {
      $$invalidate(26, activeTabLabel = activeTab === "daemon" ? "Daemon" : activeTab.startsWith("keyer:") ? (activeKeyer == null ? void 0 : activeKeyer.name) || activeSerial || "Keyer" : "");
    }
    if ($$self.$$.dirty[0] & /*activeMenuMeta, activeTab*/
    134217760) {
      $$invalidate(16, activePageTitle = (activeMenuMeta == null ? void 0 : activeMenuMeta.title) || (activeTab.startsWith("keyer:") ? "Keyer" : ""));
    }
    if ($$self.$$.dirty[0] & /*activeTabLabel, activeMenuMeta*/
    201326592) {
      $$invalidate(15, breadcrumb = activeTabLabel ? (activeMenuMeta == null ? void 0 : activeMenuMeta.label) ? `${activeTabLabel} >> ${activeMenuMeta.label}` : activeTabLabel : "");
    }
    if ($$self.$$.dirty[0] & /*activeTab, activeKeyerMenus*/
    33554464) {
      $$invalidate(14, sideMenuItems = activeTab === "daemon" ? daemonMenus : activeTab.startsWith("keyer:") ? activeKeyerMenus : []);
    }
  };
  return [
    devices,
    configDevices,
    metadata,
    keyers,
    loading,
    activeTab,
    activeMenu,
    activeSerial,
    activeKeyer,
    activeMenuId,
    runtime,
    daemonCfg,
    connectors,
    error,
    sideMenuItems,
    breadcrumb,
    activePageTitle,
    activeKeyerFlags,
    tabs,
    setTab,
    refreshDeviceConfig,
    fwString,
    reloadData,
    lastSerial,
    hashRestoredMenu,
    activeKeyerMenus,
    activeTabLabel,
    activeMenuMeta,
    select_handler,
    select_handler_1,
    daemonsettings_daemonCfg_binding,
    keyerpage_configDevices_binding
  ];
}
class App extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance, create_fragment, safe_not_equal, {}, null, [-1, -1]);
  }
}
new App({
  target: document.getElementById("app")
});
