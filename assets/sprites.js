#!/usr/bin/env node
"use strict";

const fs = require('fs')
const cp = require('child_process');

const nsg = require("node-sprite-generator");

function sorted_identity(e) {
  return e;
}
function sorted_sorter(a, b) {
  return (a.k > b.k) ? 1 : ((a.k < b.k) ? -1 : (a.i - b.i));
}
function sorted_unmapper(e) {
  return e.v;
}
const sorted = function(array, keyfn) {
  if (!keyfn) {
    keyfn = sorted_identity;
  }
  let mapped = array.map((e, i) => {
    return {k: keyfn(e), i: i, v: e}
  });
  mapped.sort(sorted_sorter);
  return mapped.map(sorted_unmapper);
}

function stylesheeter(prefix, layout, filePath, spritePath, options, callback) {
  try {
    let reg = [], dbl = [];
    sorted(layout.images, e => e.path).forEach(i => {
      let name = i.path.replace(".png", "").replace("_", "-");
      let dst = reg;
      if (name.endsWith("@2x")) {
        name = name.slice(0, -3) + "-2x";
        dst = dbl;
      }
      let x = i.x, y = i.y, h = i.height, w = i.width;
      let dx = x + w, dy = y + h;
      let rule = `  --${prefix}-${name}: rect(${y}px, ${dx}px, ${dy}px, ${x}px);`;
      dst.push(rule);
    });
    let rv = `#MinTrayR_toolbaritem {\n${reg.join('\n')}\n}\n`;
    if (dbl.length) {
      rv += `@media (min-resolution: 2dppx) {\n#MinTrayR_toolbaritem {\n${reg.join('\n')}\n}\n}\n`;
    }
    fs.writeFile(filePath, rv, callback);
    console.log("sprite", spritePath, layout.width, layout.height);
  }
  catch (ex) {
    callback(ex);
  }
}

function nsgp(src, path, sheeter) {
  return new Promise((resolve, reject) => {
    try {
      nsg({
        src: [src],
        spritePath: `${path}.png`,
        stylesheet: sheeter,
        stylesheetPath: `${path}.css`,
        layout: "packed",
        layoutOptions: {
          padding: 16,
        }
      }, err => {
        if (err) {
          reject(err, path);
        }
        else {
          resolve(path);
        }
      });
    }
    catch (ex) {
      console.error("hurrdurr");
      reject(ex, path);
    }
  });
}

function optipng(path) {
  return new Promise((reject, resolve) => {
    try {
      console.log("optipng pass");
      cp.execSync(`optipng -strip all -o7 ${path}.png`, {stdio: [0, 1, 2]});
      resolve(path);
    }
    catch (ex) {
      reject(ex, path);
    }
  });
}

console.log("creating sprites");
nsgp("icon*.png", "toolbar", stylesheeter.bind(null, "mintrayr")).then(optipng).
  then(path => console.log(path, done), console.error);
