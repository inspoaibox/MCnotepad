// Minimal WHATWG-like DOM adapter for Showdown.makeMarkdown.
// It intentionally implements only the DOM surface used by Showdown 2.1.0.
(function (global) {
  'use strict';
  var voidTags = {area:1,base:1,br:1,col:1,embed:1,hr:1,img:1,input:1,link:1,meta:1,param:1,source:1,track:1,wbr:1};
  function escapeText(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
  function escapeAttr(s) { return escapeText(s).replace(/"/g,'&quot;'); }
  function Text(value) { this.nodeType = 3; this.nodeValue = value; this.data = value; this.parentNode = null; }
  Object.defineProperty(Text.prototype, 'outerHTML', { get: function () { return escapeText(this.nodeValue); } });
  Object.defineProperty(Text.prototype, 'textContent', { get: function () { return this.nodeValue; }, set: function (v) { this.nodeValue = String(v); this.data = this.nodeValue; } });
  function Comment(value) { this.nodeType = 8; this.data = value; this.nodeValue = value; this.parentNode = null; }
  Object.defineProperty(Comment.prototype, 'outerHTML', { get: function () { return '<!--' + this.data + '-->'; } });
  function Element(tag) {
    this.nodeType = 1; this.tagName = String(tag).toUpperCase(); this.nodeName = this.tagName;
    this.childNodes = []; this.parentNode = null; this._attrs = {};
  }
  Element.prototype.hasChildNodes = function () { return this.childNodes.length > 0; };
  Element.prototype.removeChild = function (child) { var i = this.childNodes.indexOf(child); if (i >= 0) { this.childNodes.splice(i,1); child.parentNode = null; } return child; };
  Element.prototype.appendChild = function (child) { if (child.parentNode) child.parentNode.removeChild(child); child.parentNode = this; this.childNodes.push(child); return child; };
  Element.prototype.setAttribute = function (key, value) { this._attrs[String(key).toLowerCase()] = String(value); };
  Element.prototype.getAttribute = function (key) { key = String(key).toLowerCase(); return Object.prototype.hasOwnProperty.call(this._attrs,key) ? this._attrs[key] : null; };
  Element.prototype.querySelectorAll = function (selector) {
    var parts = String(selector).toLowerCase().split('>'), out = [], root = this;
    function matches(node, name) { return node && node.nodeType === 1 && node.tagName.toLowerCase() === name; }
    function visit(node, depth) {
      if (!node || !node.childNodes) return;
      for (var i=0; i<node.childNodes.length; ++i) {
        var child = node.childNodes[i];
        if (matches(child, parts[depth])) {
          if (depth === parts.length - 1) out.push(child);
          else visit(child, depth + 1);
        }
        if (depth === 0) visit(child, 0);
      }
    }
    visit(root, 0); return out;
  };
  Object.defineProperty(Element.prototype, 'className', { get: function () { return this.getAttribute('class') || ''; }, set: function (v) { this.setAttribute('class', v); } });
  Object.defineProperty(Element.prototype, 'childElementCount', { get: function () { var n=0; for (var i=0;i<this.childNodes.length;++i) n += this.childNodes[i].nodeType === 1 ? 1 : 0; return n; } });
  Object.defineProperty(Element.prototype, 'firstChild', { get: function () { return this.childNodes.length ? this.childNodes[0] : null; } });
  Object.defineProperty(Element.prototype, 'textContent', { get: function () { var s=''; for (var i=0;i<this.childNodes.length;++i) s += this.childNodes[i].textContent || ''; return s; }, set: function (v) { this.childNodes=[]; this.appendChild(new Text(String(v))); } });
  function serialize(node) { var s = '<' + node.tagName.toLowerCase(); for (var k in node._attrs) if (Object.prototype.hasOwnProperty.call(node._attrs,k)) s += ' ' + k + '="' + escapeAttr(node._attrs[k]) + '"'; if (voidTags[node.tagName.toLowerCase()]) return s + '>'; s += '>'; for (var i=0;i<node.childNodes.length;++i) s += node.childNodes[i].outerHTML; return s + '</' + node.tagName.toLowerCase() + '>'; }
  Object.defineProperty(Element.prototype, 'outerHTML', { get: function () { return serialize(this); }, set: function (html) { if (this.parentNode) { var p=this.parentNode, i=p.childNodes.indexOf(this); p.removeChild(this); var holder=parse(String(html)); for (var n=0;n<holder.childNodes.length;++n) { var c=holder.childNodes[n]; c.parentNode=p; p.childNodes.splice(i++,0,c); } } } });
  Object.defineProperty(Element.prototype, 'innerHTML', { get: function () { var s=''; for (var i=0;i<this.childNodes.length;++i) s += this.childNodes[i].outerHTML; return s; }, set: function (html) { this.childNodes=[]; var holder=parse(String(html)); for (var i=0;i<holder.childNodes.length;++i) this.appendChild(holder.childNodes[i]); } });
  function decode(s) { return s.replace(/&(#x[0-9a-f]+|#\d+|[a-z]+);/gi, function(_, x) { if (x.toLowerCase()==='amp') return '&'; if (x.toLowerCase()==='lt') return '<'; if (x.toLowerCase()==='gt') return '>'; if (x.toLowerCase()==='quot') return '"'; if (x.toLowerCase()==='apos') return "'"; if (x.charAt(0)==='#') { var n=x.charAt(1).toLowerCase()==='x' ? parseInt(x.slice(2),16) : parseInt(x.slice(1),10); return isNaN(n) ? _ : String.fromCharCode(n); } return _; }); }
  function parse(html) {
    var root = new Element('div'), stack=[root], re=/<!--[\s\S]*?-->|<![^>]*>|<\/?[A-Za-z][^>]*>/g, pos=0, m;
    function text(v) { if (v) stack[stack.length-1].appendChild(new Text(decode(v))); }
    while ((m=re.exec(html)) !== null) {
      text(html.slice(pos,m.index)); var token=m[0]; pos=re.lastIndex;
      if (token.indexOf('<!--')===0) { stack[stack.length-1].appendChild(new Comment(token.slice(4,-3))); continue; }
      if (token.charAt(1)==='!') continue;
      if (token.charAt(1)==='/') { var close=token.slice(2,-1).trim().toLowerCase(); for (var i=stack.length-1;i>0;--i) if (stack[i].tagName.toLowerCase()===close) { stack.length=i; break; } continue; }
      var body=token.slice(1,-1), self=/\/\s*$/.test(body); body=body.replace(/\/\s*$/,''); var mt=/^\s*([^\s/>]+)/.exec(body); if (!mt) continue; var el=new Element(mt[1]); var rest=body.slice(mt[0].length), ar=/([^\s=]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'=<>`]+)))?/g, am; while ((am=ar.exec(rest))!==null) el.setAttribute(am[1], am[2] !== undefined ? am[2] : am[3] !== undefined ? am[3] : am[4] !== undefined ? am[4] : ''); stack[stack.length-1].appendChild(el); if (!self && !voidTags[el.tagName.toLowerCase()]) stack.push(el);
    }
    text(html.slice(pos)); return root;
  }
  global.MCShowdownDOM = { createElement: function (tag) { return new Element(tag); } };
  global.MCShowdownDOM.implementation = { createHTMLDocument: function () { return global.MCShowdownDOM; } };
  global.document = global.MCShowdownDOM;
  global.window = global;
}(this));
