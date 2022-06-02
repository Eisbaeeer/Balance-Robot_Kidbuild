/*
 *  Created on: 23.02.2021
 *      Author: anonymous
 * 
 *  Changelog:
 *    ### Version 1.0
 *      (20220322)
 *      - Ported to platformio
 *      - Bugfix Android app right/left direction
 * 
 *  ToDo:
 *    - Web-GUI
 *    - OTA
 * 
 * ******** Pin out **********
 * Pin 12   Stepper A&B Enable
 * Pin 26   Stepper A STEP
 * Pin 25   Stepper A DIR
 * Pin 14   Stepper B STEP
 * Pin 27   Stepper B DIR
 * 
 * Pin 13   Servo
 * 
 * Pin 21   MPU-6050 SDA
 * Pin 22   MPU-6050 SCL
 * 
 * Pin 32   LED Light
 * 
 * Pin 33   BUZZER
 * 
 * ******** Command Character *******
 * Forward              /?fader1=1.00
 * Backward             /?fader1=0.00
 * Turn Right           /?fader2=0.00
 * Turn Left            /?fader2=1.00
 * Servo Arm            /?push1=1|2
 * Beep On|Off          /?push3=1|2
 * Turn Light On|Off    /?push4=1|0
 * Mode PRO On|Off      /?toggle1=1|0
 * P-Stability          /?fader3=0.00-1.00
 * D-Stability          /?fader4=0.00-1.00
 * P-Speed              /?fader5=0.00-1.00
 * I-Speed              /?fader6=0.00-1.00
 * 
 */
 
#include <Wire.h>
#include <WiFi.h>
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <AsyncElegantOTA.h>
#include "Control.h"
#include "MPU6050.h"
#include "Motors.h"
#include "defines.h"
#include "globals.h"
#include <stdio.h>
#include "esp_types.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/ledc.h"
#include "esp32-hal-ledc.h"

const char* PARAM_FADER1 = "fader1";
const char* PARAM_FADER2 = "fader2";
const char* PARAM_PUSH1 = "push1";
const char* PARAM_PUSH2 = "push2";
const char* PARAM_PUSH3 = "push3";
const char* PARAM_PUSH4 = "push4";
const char* PARAM_TOGGLE1 = "toggle1";
const char* PARAM_FADER3 = "fader3";
const char* PARAM_FADER4 = "fader4";
const char* PARAM_FADER5 = "fader5";
const char* PARAM_FADER6 = "fader6";
const char joystick_html[] PROGMEM="<html>\n\t<head>\n\t\t<meta charset=\"utf-8\">\n\t\t<meta name=\"viewport\" content=\"width=device-width, user-scalable=no, minimum-scale=1.0, maximum-scale=1.0\">\t\t\n\t\t<style>\n\t\tbody {\n\t\t\toverflow\t: hidden;\n\t\t\tpadding\t\t: 0;\n\t\t\tmargin\t\t: 0;\n\t\t\tbackground-color: #BBB;\n\t\t}\n\t\t#info {\n\t\t\tposition\t: absolute;\n\t\t\ttop\t\t: 0px;\n\t\t\twidth\t\t: 100%;\n\t\t\tpadding\t\t: 5px;\n\t\t\ttext-align\t: center;\n\t\t}\n\t\t#info a {\n\t\t\tcolor\t\t: #66F;\n\t\t\ttext-decoration\t: none;\n\t\t}\n\t\t#info a:hover {\n\t\t\ttext-decoration\t: underline;\n\t\t}\n\t\t#container {\n\t\t\twidth\t\t: 100%;\n\t\t\theight\t\t: 100%;\n\t\t\toverflow\t: hidden;\n\t\t\tpadding\t\t: 0;\n\t\t\tmargin\t\t: 0;\n\t\t\t-webkit-user-select\t: none;\n\t\t\t-moz-user-select\t: none;\n\t\t}\n\t\t</style>\n\n\t</head>\n\t<body>\n\n\t\n\t\t<div id=\"container\"></div>\n\t\t<div id=\"info\">\n\t\t\tTouch screen to control!\n\t\t\t<br><b><hr><button onclick=\"window.location.href = '/web/function1.html'\">Licht / Light</button>\n\t\t\t<p><b><button onclick=\"window.location.href = '/web/function2.html'\">Beep</button>\n\t\t\t<p><b><button onclick=\"window.location.href = '/web/function3.html'\">Servo Arm</button>\n\t\t\t<br>\n\t\t\t</b><br/><hr>\n\t\t\t<span id=\"result\"></span>\n\t\t</div> \n\t\t<script src=\"./virtualjoystick.js\"></script>\n\t\t<script>\n\t\t\tconsole.log(\"touchscreen is\", VirtualJoystick.touchScreenAvailable() ? \"available\" : \"not available\");\n\t\n\t\t\tvar joystick\t= new VirtualJoystick({\n\t\t\t\tcontainer\t: document.getElementById('container'),\n\t\t\t\tmouseSupport\t: true,\n\t\t\t\tlimitStickTravel\t: true,\n\t\t\t});\n\t\t\tjoystick.addEventListener('touchStart', function(){\n\t\t\t\tconsole.log('down')\n\t\t\t})\n\t\t\tjoystick.addEventListener('touchEnd', function(){\n\t\t\t\tconsole.log('up')\n\t\t\t})\n\t\t\tvar prevX = 0;\n\t\t\tvar prevY = 0;\n\t\t\tvar newX = 0;\n\t\t\tvar newY = 0;\n\t\t\tsetInterval(function(){\n\t\t\t\tvar outputEl\t= document.getElementById('result');\n\t\t\t\tnewX = Math.round(joystick.deltaX());\n\t\t\t\tnewY = Math.round(joystick.deltaY()) * -1;\n\t\t\t\toutputEl.innerHTML\t= '<b>Position:</b> '\n\t\t\t\t\t+ ' X:'+newX\n\t\t\t\t\t+ ' Y:'+newY;\n\t\t\t\tif ( newX != prevX || newY != prevY || newX!=0 || newY!=0){\n\t\t\t\t\tvar xhr = new XMLHttpRequest();\n\t\t\t\t\txhr.open('PUT', \"./jsData.html?x=\"+newX+\"&y=\"+newY)\n\t\t\t\t\txhr.send();\n\t\t\t\t\tsetTimeout(function(){xhr.abort();},100);\n\t\t\t\t}\n\t\t\t\tprevX = newX;\n\t\t\t\tprevY = newY;\n\t\t\t}, 150);\n\t\t</script>\n\t</body>\n</html>";
const char virtualjoystick_js[] PROGMEM="var VirtualJoystick\t= function(opts)\n{\n\topts\t\t\t= opts\t\t\t|| {};\n\tthis._container\t\t= opts.container\t|| document.body;\n\tthis._strokeStyle\t= opts.strokeStyle\t|| 'cyan';\n\tthis._stickEl\t\t= opts.stickElement\t|| this._buildJoystickStick();\n\tthis._baseEl\t\t= opts.baseElement\t|| this._buildJoystickBase();\n\tthis._mouseSupport\t= opts.mouseSupport !== undefined ? opts.mouseSupport : false;\n\tthis._stationaryBase\t= opts.stationaryBase || false;\n\tthis._baseX\t\t= this._stickX = opts.baseX || 0\n\tthis._baseY\t\t= this._stickY = opts.baseY || 0\n\tthis._limitStickTravel\t= opts.limitStickTravel || false\n\tthis._stickRadius\t= opts.stickRadius !== undefined ? opts.stickRadius : 100\n\tthis._useCssTransform\t= opts.useCssTransform !== undefined ? opts.useCssTransform : false\n\n\tthis._container.style.position\t= \"relative\"\n\n\tthis._container.appendChild(this._baseEl)\n\tthis._baseEl.style.position\t= \"absolute\"\n\tthis._baseEl.style.display\t= \"none\"\n\tthis._container.appendChild(this._stickEl)\n\tthis._stickEl.style.position\t= \"absolute\"\n\tthis._stickEl.style.display\t= \"none\"\n\n\tthis._pressed\t= false;\n\tthis._touchIdx\t= null;\n\t\n\tif(this._stationaryBase === true){\n\t\tthis._baseEl.style.display\t= \"\";\n\t\tthis._baseEl.style.left\t\t= (this._baseX - this._baseEl.width /2)+\"px\";\n\t\tthis._baseEl.style.top\t\t= (this._baseY - this._baseEl.height/2)+\"px\";\n\t}\n    \n\tthis._transform\t= this._useCssTransform ? this._getTransformProperty() : false;\n\tthis._has3d\t= this._check3D();\n\t\n\tvar __bind\t= function(fn, me){ return function(){ return fn.apply(me, arguments); }; };\n\tthis._$onTouchStart\t= __bind(this._onTouchStart\t, this);\n\tthis._$onTouchEnd\t= __bind(this._onTouchEnd\t, this);\n\tthis._$onTouchMove\t= __bind(this._onTouchMove\t, this);\n\tthis._container.addEventListener( 'touchstart'\t, this._$onTouchStart\t, false );\n\tthis._container.addEventListener( 'touchend'\t, this._$onTouchEnd\t, false );\n\tthis._container.addEventListener( 'touchmove'\t, this._$onTouchMove\t, false );\n\tif( this._mouseSupport ){\n\t\tthis._$onMouseDown\t= __bind(this._onMouseDown\t, this);\n\t\tthis._$onMouseUp\t= __bind(this._onMouseUp\t, this);\n\t\tthis._$onMouseMove\t= __bind(this._onMouseMove\t, this);\n\t\tthis._container.addEventListener( 'mousedown'\t, this._$onMouseDown\t, false );\n\t\tthis._container.addEventListener( 'mouseup'\t, this._$onMouseUp\t, false );\n\t\tthis._container.addEventListener( 'mousemove'\t, this._$onMouseMove\t, false );\n\t}\n}\n\nVirtualJoystick.prototype.destroy\t= function()\n{\n\tthis._container.removeChild(this._baseEl);\n\tthis._container.removeChild(this._stickEl);\n\n\tthis._container.removeEventListener( 'touchstart'\t, this._$onTouchStart\t, false );\n\tthis._container.removeEventListener( 'touchend'\t\t, this._$onTouchEnd\t, false );\n\tthis._container.removeEventListener( 'touchmove'\t, this._$onTouchMove\t, false );\n\tif( this._mouseSupport ){\n\t\tthis._container.removeEventListener( 'mouseup'\t\t, this._$onMouseUp\t, false );\n\t\tthis._container.removeEventListener( 'mousedown'\t, this._$onMouseDown\t, false );\n\t\tthis._container.removeEventListener( 'mousemove'\t, this._$onMouseMove\t, false );\n\t}\n}\n\n/**\n * @returns {Boolean} true if touchscreen is currently available, false otherwise\n*/\nVirtualJoystick.touchScreenAvailable\t= function()\n{\n\treturn 'createTouch' in document ? true : false;\n}\n\n/**\n * microevents.js - https://github.com/jeromeetienne/microevent.js\n*/\n;(function(destObj){\n\tdestObj.addEventListener\t= function(event, fct){\n\t\tif(this._events === undefined) \tthis._events\t= {};\n\t\tthis._events[event] = this._events[event]\t|| [];\n\t\tthis._events[event].push(fct);\n\t\treturn fct;\n\t};\n\tdestObj.removeEventListener\t= function(event, fct){\n\t\tif(this._events === undefined) \tthis._events\t= {};\n\t\tif( event in this._events === false  )\treturn;\n\t\tthis._events[event].splice(this._events[event].indexOf(fct), 1);\n\t};\n\tdestObj.dispatchEvent\t\t= function(event /* , args... */){\n\t\tif(this._events === undefined) \tthis._events\t= {};\n\t\tif( this._events[event] === undefined )\treturn;\n\t\tvar tmpArray\t= this._events[event].slice(); \n\t\tfor(var i = 0; i < tmpArray.length; i++){\n\t\t\tvar result\t= tmpArray[i].apply(this, Array.prototype.slice.call(arguments, 1))\n\t\t\tif( result !== undefined )\treturn result;\n\t\t}\n\t\treturn undefined\n\t};\n})(VirtualJoystick.prototype);\n\n//////////////////////////////////////////////////////////////////////////////////\n//\t\t\t\t\t\t\t\t\t\t//\n//////////////////////////////////////////////////////////////////////////////////\n\nVirtualJoystick.prototype.deltaX\t= function(){ return this._stickX - this._baseX;\t}\nVirtualJoystick.prototype.deltaY\t= function(){ return this._stickY - this._baseY;\t}\n\nVirtualJoystick.prototype.up\t= function(){\n\tif( this._pressed === false )\treturn false;\n\tvar deltaX\t= this.deltaX();\n\tvar deltaY\t= this.deltaY();\n\tif( deltaY >= 0 )\t\t\t\treturn false;\n\tif( Math.abs(deltaX) > 2*Math.abs(deltaY) )\treturn false;\n\treturn true;\n}\nVirtualJoystick.prototype.down\t= function(){\n\tif( this._pressed === false )\treturn false;\n\tvar deltaX\t= this.deltaX();\n\tvar deltaY\t= this.deltaY();\n\tif( deltaY <= 0 )\t\t\t\treturn false;\n\tif( Math.abs(deltaX) > 2*Math.abs(deltaY) )\treturn false;\n\treturn true;\t\n}\nVirtualJoystick.prototype.right\t= function(){\n\tif( this._pressed === false )\treturn false;\n\tvar deltaX\t= this.deltaX();\n\tvar deltaY\t= this.deltaY();\n\tif( deltaX <= 0 )\t\t\t\treturn false;\n\tif( Math.abs(deltaY) > 2*Math.abs(deltaX) )\treturn false;\n\treturn true;\t\n}\nVirtualJoystick.prototype.left\t= function(){\n\tif( this._pressed === false )\treturn false;\n\tvar deltaX\t= this.deltaX();\n\tvar deltaY\t= this.deltaY();\n\tif( deltaX >= 0 )\t\t\t\treturn false;\n\tif( Math.abs(deltaY) > 2*Math.abs(deltaX) )\treturn false;\n\treturn true;\t\n}\n\n//////////////////////////////////////////////////////////////////////////////////\n//\t\t\t\t\t\t\t\t\t\t//\n//////////////////////////////////////////////////////////////////////////////////\n\nVirtualJoystick.prototype._onUp\t= function()\n{\n\tthis._pressed\t= false; \n\tthis._stickEl.style.display\t= \"none\";\n\t\n\tif(this._stationaryBase == false){\t\n\t\tthis._baseEl.style.display\t= \"none\";\n\t\n\t\tthis._baseX\t= this._baseY\t= 0;\n\t\tthis._stickX\t= this._stickY\t= 0;\n\t}\n}\n\nVirtualJoystick.prototype._onDown\t= function(x, y)\n{\n\tthis._pressed\t= true; \n\tif(this._stationaryBase == false){\n\t\tthis._baseX\t= x;\n\t\tthis._baseY\t= y;\n\t\tthis._baseEl.style.display\t= \"\";\n\t\tthis._move(this._baseEl.style, (this._baseX - this._baseEl.width /2), (this._baseY - this._baseEl.height/2));\n\t}\n\t\n\tthis._stickX\t= x;\n\tthis._stickY\t= y;\n\t\n\tif(this._limitStickTravel === true){\n\t\tvar deltaX\t= this.deltaX();\n\t\tvar deltaY\t= this.deltaY();\n\t\tvar stickDistance = Math.sqrt( (deltaX * deltaX) + (deltaY * deltaY) );\n\t\tif(stickDistance > this._stickRadius){\n\t\t\tvar stickNormalizedX = deltaX / stickDistance;\n\t\t\tvar stickNormalizedY = deltaY / stickDistance;\n\t\t\t\n\t\t\tthis._stickX = stickNormalizedX * this._stickRadius + this._baseX;\n\t\t\tthis._stickY = stickNormalizedY * this._stickRadius + this._baseY;\n\t\t} \t\n\t}\n\t\n\tthis._stickEl.style.display\t= \"\";\n\tthis._move(this._stickEl.style, (this._stickX - this._stickEl.width /2), (this._stickY - this._stickEl.height/2));\t\n}\n\nVirtualJoystick.prototype._onMove\t= function(x, y)\n{\n\tif( this._pressed === true ){\n\t\tthis._stickX\t= x;\n\t\tthis._stickY\t= y;\n\t\t\n\t\tif(this._limitStickTravel === true){\n\t\t\tvar deltaX\t= this.deltaX();\n\t\t\tvar deltaY\t= this.deltaY();\n\t\t\tvar stickDistance = Math.sqrt( (deltaX * deltaX) + (deltaY * deltaY) );\n\t\t\tif(stickDistance > this._stickRadius){\n\t\t\t\tvar stickNormalizedX = deltaX / stickDistance;\n\t\t\t\tvar stickNormalizedY = deltaY / stickDistance;\n\t\t\t\n\t\t\t\tthis._stickX = stickNormalizedX * this._stickRadius + this._baseX;\n\t\t\t\tthis._stickY = stickNormalizedY * this._stickRadius + this._baseY;\n\t\t\t} \t\t\n\t\t}\n\t\t\n        \tthis._move(this._stickEl.style, (this._stickX - this._stickEl.width /2), (this._stickY - this._stickEl.height/2));\t\n\t}\t\n}\n\n\n//////////////////////////////////////////////////////////////////////////////////\n//\t\tbind touch events (and mouse events for debug)\t\t\t//\n//////////////////////////////////////////////////////////////////////////////////\n\nVirtualJoystick.prototype._onMouseUp\t= function(event)\n{\n\treturn this._onUp();\n}\n\nVirtualJoystick.prototype._onMouseDown\t= function(event)\n{\n\tevent.preventDefault();\n\tvar x\t= event.clientX;\n\tvar y\t= event.clientY;\n\treturn this._onDown(x, y);\n}\n\nVirtualJoystick.prototype._onMouseMove\t= function(event)\n{\n\tvar x\t= event.clientX;\n\tvar y\t= event.clientY;\n\treturn this._onMove(x, y);\n}\n\n//////////////////////////////////////////////////////////////////////////////////\n//\t\tcomment\t\t\t\t\t\t\t\t//\n//////////////////////////////////////////////////////////////////////////////////\n\nVirtualJoystick.prototype._onTouchStart\t= function(event)\n{\n\t// if there is already a touch inprogress do nothing\n\tif( this._touchIdx !== null )\treturn;\n\n\t// notify event for validation\n\tvar isValid\t= this.dispatchEvent('touchStartValidation', event);\n\tif( isValid === false )\treturn;\n\t\n\t// dispatch touchStart\n\tthis.dispatchEvent('touchStart', event);\n\n\tevent.preventDefault();\n\t// get the first who changed\n\tvar touch\t= event.changedTouches[0];\n\t// set the touchIdx of this joystick\n\tthis._touchIdx\t= touch.identifier;\n\n\t// forward the action\n\tvar x\t\t= touch.pageX;\n\tvar y\t\t= touch.pageY;\n\treturn this._onDown(x, y)\n}\n\nVirtualJoystick.prototype._onTouchEnd\t= function(event)\n{\n\t// if there is no touch in progress, do nothing\n\tif( this._touchIdx === null )\treturn;\n\n\t// dispatch touchEnd\n\tthis.dispatchEvent('touchEnd', event);\n\n\t// try to find our touch event\n\tvar touchList\t= event.changedTouches;\n\tfor(var i = 0; i < touchList.length && touchList[i].identifier !== this._touchIdx; i++);\n\t// if touch event isnt found, \n\tif( i === touchList.length)\treturn;\n\n\t// reset touchIdx - mark it as no-touch-in-progress\n\tthis._touchIdx\t= null;\n\n//??????\n// no preventDefault to get click event on ios\nevent.preventDefault();\n\n\treturn this._onUp()\n}\n\nVirtualJoystick.prototype._onTouchMove\t= function(event)\n{\n\t// if there is no touch in progress, do nothing\n\tif( this._touchIdx === null )\treturn;\n\n\t// try to find our touch event\n\tvar touchList\t= event.changedTouches;\n\tfor(var i = 0; i < touchList.length && touchList[i].identifier !== this._touchIdx; i++ );\n\t// if touch event with the proper identifier isnt found, do nothing\n\tif( i === touchList.length)\treturn;\n\tvar touch\t= touchList[i];\n\n\tevent.preventDefault();\n\n\tvar x\t\t= touch.pageX;\n\tvar y\t\t= touch.pageY;\n\treturn this._onMove(x, y)\n}\n\n\n//////////////////////////////////////////////////////////////////////////////////\n//\t\tbuild default stickEl and baseEl\t\t\t\t//\n//////////////////////////////////////////////////////////////////////////////////\n\n/**\n * build the canvas for joystick base\n */\nVirtualJoystick.prototype._buildJoystickBase\t= function()\n{\n\tvar canvas\t= document.createElement( 'canvas' );\n\tcanvas.width\t= 126;\n\tcanvas.height\t= 126;\n\t\n\tvar ctx\t\t= canvas.getContext('2d');\n\tctx.beginPath(); \n\tctx.strokeStyle = this._strokeStyle; \n\tctx.lineWidth\t= 6; \n\tctx.arc( canvas.width/2, canvas.width/2, 40, 0, Math.PI*2, true); \n\tctx.stroke();\t\n\n\tctx.beginPath(); \n\tctx.strokeStyle\t= this._strokeStyle; \n\tctx.lineWidth\t= 2; \n\tctx.arc( canvas.width/2, canvas.width/2, 60, 0, Math.PI*2, true); \n\tctx.stroke();\n\t\n\treturn canvas;\n}\n\n/**\n * build the canvas for joystick stick\n */\nVirtualJoystick.prototype._buildJoystickStick\t= function()\n{\n\tvar canvas\t= document.createElement( 'canvas' );\n\tcanvas.width\t= 86;\n\tcanvas.height\t= 86;\n\tvar ctx\t\t= canvas.getContext('2d');\n\tctx.beginPath(); \n\tctx.strokeStyle\t= this._strokeStyle; \n\tctx.lineWidth\t= 6; \n\tctx.arc( canvas.width/2, canvas.width/2, 40, 0, Math.PI*2, true); \n\tctx.stroke();\n\treturn canvas;\n}\n\n//////////////////////////////////////////////////////////////////////////////////\n//\t\tmove using translate3d method with fallback to translate > 'top' and 'left'\t\t\n//      modified from https://github.com/component/translate and dependents\n//////////////////////////////////////////////////////////////////////////////////\n\nVirtualJoystick.prototype._move = function(style, x, y)\n{\n\tif (this._transform) {\n\t\tif (this._has3d) {\n\t\t\tstyle[this._transform] = 'translate3d(' + x + 'px,' + y + 'px, 0)';\n\t\t} else {\n\t\t\tstyle[this._transform] = 'translate(' + x + 'px,' + y + 'px)';\n\t\t}\n\t} else {\n\t\tstyle.left = x + 'px';\n\t\tstyle.top = y + 'px';\n\t}\n}\n\nVirtualJoystick.prototype._getTransformProperty = function() \n{\n\tvar styles = [\n\t\t'webkitTransform',\n\t\t'MozTransform',\n\t\t'msTransform',\n\t\t'OTransform',\n\t\t'transform'\n\t];\n\n\tvar el = document.createElement('p');\n\tvar style;\n\n\tfor (var i = 0; i < styles.length; i++) {\n\t\tstyle = styles[i];\n\t\tif (null != el.style[style]) {\n\t\t\treturn style;\n\t\t}\n\t}         \n}\n  \nVirtualJoystick.prototype._check3D = function() \n{        \n\tvar prop = this._getTransformProperty();\n\t// IE8<= doesn't have `getComputedStyle`\n\tif (!prop || !window.getComputedStyle) return module.exports = false;\n\n\tvar map = {\n\t\twebkitTransform: '-webkit-transform',\n\t\tOTransform: '-o-transform',\n\t\tmsTransform: '-ms-transform',\n\t\tMozTransform: '-moz-transform',\n\t\ttransform: 'transform'\n\t};\n\n\t// from: https://gist.github.com/lorenzopolidori/3794226\n\tvar el = document.createElement('div');\n\tel.style[prop] = 'translate3d(1px,1px,1px)';\n\tdocument.body.insertBefore(el, null);\n\tvar val = getComputedStyle(el).getPropertyValue(map[prop]);\n\tdocument.body.removeChild(el);\n\tvar exports = null != val && val.length && 'none' != val;\n\treturn exports;\n}\n";

// Struct for tasks
struct task
{
    unsigned long rate;
    unsigned long previous;
};

task taskA = {.rate = 300, .previous = 0};      // 500 ms

/* Wifi Crdentials */
String sta_ssid = "<your Wifi SSID>";     // set Wifi network you want to connect to
String sta_password = "<your Wifi passwor>";        // set password for Wifi network

unsigned long previousMillis = 0;

AsyncWebServer server(80);

void initMPU6050() {
  MPU6050_setup();
  delay(500);
  MPU6050_calibrate();
}

void initTimers();

void playsound() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(150);
  digitalWrite(PIN_BUZZER, LOW);
  delay(80);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(150);
  digitalWrite(PIN_BUZZER, LOW);
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void handleVirtualJoystickJS(AsyncWebServerRequest *request){
  request->send(200,"application/javascript",virtualjoystick_js);
}

void handleJoystickHtml(AsyncWebServerRequest *request)
{
 request->send(200,"text/html",joystick_html); 
}

//This function takes the parameters passed in the URL(the x and y coordinates of the joystick)
//and sets the motor speed based on those parameters. 

void handleFunktion1(AsyncWebServerRequest *request) {
  request->send(200,"text/html",joystick_html);
  Serial.println("Funktion1 aufgerufen ...");
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

void handleFunktion2(AsyncWebServerRequest *request) {
  request->send(200,"text/html",joystick_html); 
  Serial.println("Funktion2 aufgerufen ...");
  playsound();
}

void handleFunktion3(AsyncWebServerRequest *request) {
  request->send(200,"text/html",joystick_html); 
  Serial.println("Funktion3 aufgerufen ...");
  taskA.previous = millis();
    fromWeb = 1;
    OSCpage = 1;
    OSCpush[0]=1;
  }

void handleJSData(AsyncWebServerRequest *request) {
  
  //int x = server.arg(0);
  String inputValue;
  inputValue = request->getParam("x")->value();
  int xpos = inputValue.toFloat(); 
  Serial.print(F("X: "));
  Serial.print(inputValue);
  
  inputValue = request->getParam("y")->value();
  int ypos = inputValue.toFloat(); 
  Serial.print(F(" Y: "));
  Serial.println(inputValue);
  

  // Calculate Forward/Backward Left/Right
  if (xpos == 0 ) {
    OSCfader[1] = 0.5;
  } else if ((xpos > 1) && (xpos < 21)) {
    OSCfader[1] = 0.6;
  } else if ((xpos > 20) && (xpos < 41)) {
    OSCfader[1] = 0.7;
  } else if ((xpos > 40) && (xpos < 61)) {
    OSCfader[1] = 0.8;
  } else if ((xpos > 60) && (xpos < 81)) {
    OSCfader[1] = 0.9;
  } else if ((xpos > 80) && (xpos < 101)) {
    OSCfader[1] = 1.0;
  } else if ((xpos < 0) && (xpos > -21)) {
    OSCfader[1] = 0.4;
  } else if ((xpos > -20) && (xpos > -41)) {
    OSCfader[1] = 0.3;
  } else if ((xpos > -40) && (xpos > -61)) {
    OSCfader[1] = 0.2;
  } else if ((xpos > -60) && (xpos > -81)) {
    OSCfader[1] = 0.1;
  } else if ((xpos > -80) && (xpos > -101)) {
    OSCfader[1] = 0.0;
  } 

if (ypos == 0 ) {
    OSCfader[0] = 0.5;
  } else if ((ypos > 1) && (ypos < 21)) {
    OSCfader[0] = 0.6;
  } else if ((ypos > 20) && (ypos < 41)) {
    OSCfader[0] = 0.7;
  } else if ((ypos > 40) && (ypos < 61)) {
    OSCfader[0] = 0.8;
  } else if ((ypos > 60) && (ypos < 81)) {
    OSCfader[0] = 0.9;
  } else if ((ypos > 80) && (ypos < 101)) {
    OSCfader[0] = 1.0;
  } else if ((ypos < 0) && (ypos > -21)) {
    OSCfader[0] = 0.4;
  } else if ((ypos > -20) && (ypos > -41)) {
    OSCfader[0] = 0.3;
  } else if ((ypos > -40) && (ypos > -61)) {
    OSCfader[0] = 0.2;
  } else if ((ypos > -60) && (ypos > -81)) {
    OSCfader[0] = 0.1;
  } else if ((ypos > -80) && (ypos > -101)) {
    OSCfader[0] = 0.0;
  } 

  //return an HTTP 200
  request->send(200, "text/plain", "");  

  // New Message flag
  OSCnewMessage = 1;
  OSCpage = 1; 
  
}

void setup() {
  Serial.begin(115200);         // set up seriamonitor at 115200 bps
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("*Kidbuild Balancing Robot*");
  Serial.println("--------------------------------------------------------");


  pinMode(PIN_ENABLE_MOTORS, OUTPUT);
  digitalWrite(PIN_ENABLE_MOTORS, HIGH);
  
  pinMode(PIN_MOTOR1_DIR, OUTPUT);
  pinMode(PIN_MOTOR1_STEP, OUTPUT);
  pinMode(PIN_MOTOR2_DIR, OUTPUT);
  pinMode(PIN_MOTOR2_STEP, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_WIFI_LED, OUTPUT);
  digitalWrite(PIN_WIFI_LED, LOW);
  
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  ledcSetup(6, 50, 16); // channel 6, 50 Hz, 16-bit width
  ledcAttachPin(PIN_SERVO, 6);   // GPIO 22 assigned to channel 1
  delay(50);
  ledcWrite(6, SERVO_AUX_NEUTRO);
  
  Wire.begin();
  initMPU6050();

  // Set NodeMCU Wifi hostname based on chip mac address
  char chip_id[15];
  snprintf(chip_id, 15, "%04X", (uint16_t)(ESP.getEfuseMac()>>32));
  String hostname = "Kidbuild Robot -" + String(chip_id);

  Serial.println();
  Serial.println("Hostname: "+hostname);

  // first, set NodeMCU as STA mode to connect with a Wifi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
  Serial.println("");
  Serial.print("Connecting to: ");
  Serial.println(sta_ssid);
  Serial.print("Password: ");
  Serial.println(sta_password);

  // try to connect with Wifi network about 8 seconds
  unsigned long currentMillis = millis();
  previousMillis = currentMillis;
  while (WiFi.status() != WL_CONNECTED && currentMillis - previousMillis <= 8000) {
    delay(500);
    Serial.print(".");
    currentMillis = millis();
  }

  // if failed to connect with Wifi network set NodeMCU as AP mode
  IPAddress myIP;
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("*WiFi-STA-Mode*");
    Serial.print("IP: ");
    myIP=WiFi.localIP();
    Serial.println(myIP);
    digitalWrite(PIN_WIFI_LED, HIGH);    // Wifi LED on when connected to Wifi as STA mode
    delay(2000);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(hostname.c_str());
    myIP = WiFi.softAPIP();
    Serial.println("");
    Serial.println("WiFi failed connected to " + sta_ssid);
    Serial.println("");
    Serial.println("*WiFi-AP-Mode*");
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    digitalWrite(PIN_WIFI_LED, LOW);   // Wifi LED off when status as AP mode
    delay(2000);
  }


  // Send a GET request to <ESP_IP>/?fader=<inputValue>
    server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputValue;
    String inputMessage;
    OSCnewMessage = 1;
 
    // Get value for Forward/Backward
    if (request->hasParam(PARAM_FADER1)) {
      OSCpage = 1;
      inputValue = request->getParam(PARAM_FADER1)->value();
      inputMessage = PARAM_FADER1;
      OSCfader[0] = inputValue.toFloat();
    }
    // Get value for Right/Left

    else if (request->hasParam(PARAM_FADER2)) {
      OSCpage = 1;
      inputValue = request->getParam(PARAM_FADER2)->value();
      inputMessage = PARAM_FADER2;
      OSCfader[1] = inputValue.toFloat();
      OSCfader[1] = (1 - OSCfader[1]);
    }
    // Get value for Servo0
    else if (request->hasParam(PARAM_PUSH1)) {
      OSCpage = 1;
      inputValue = request->getParam(PARAM_PUSH1)->value();
      inputMessage = PARAM_PUSH1;
      if(inputValue.equals("1")) OSCpush[0]=1;
      else OSCpush[0]=0;
    }
    // Get value for Setting
    else if (request->hasParam(PARAM_PUSH2)) {
      OSCpage = 2;
      inputValue = request->getParam(PARAM_PUSH2)->value();
      inputMessage = PARAM_PUSH2;
      if(inputValue.equals("1")) OSCpush[2]=1;
      else OSCpush[2]=0;
    }
    // Get value for Buzzer
    else if (request->hasParam(PARAM_PUSH3)) {
      inputValue = request->getParam(PARAM_PUSH3)->value();
      inputMessage = PARAM_PUSH3;
      if(inputValue.equals("1")) {
        playsound();
      }
    }
    // Get value for Led
    else if (request->hasParam(PARAM_PUSH4)) {
      inputValue = request->getParam(PARAM_PUSH4)->value();
      inputMessage = PARAM_PUSH4;
      if(inputValue.equals("1")) digitalWrite(PIN_LED, HIGH);
      else digitalWrite(PIN_LED, LOW);
    }
    // Get value for mode PRO
    else if (request->hasParam(PARAM_TOGGLE1)) {
      OSCpage = 1;
      inputValue = request->getParam(PARAM_TOGGLE1)->value();
      inputMessage = PARAM_TOGGLE1;
      if(inputValue.equals("1")) OSCtoggle[0]=1;
      else OSCtoggle[0]=0;
    }
    // Get value for P-Stability
    else if (request->hasParam(PARAM_FADER3)) {
      OSCpage = 2;
      inputValue = request->getParam(PARAM_FADER3)->value();
      inputMessage = PARAM_FADER3;
      OSCfader[0] = inputValue.toFloat();
    }
    // Get value for D-Stability
    else if (request->hasParam(PARAM_FADER4)) {
      OSCpage = 2;
      inputValue = request->getParam(PARAM_FADER4)->value();
      inputMessage = PARAM_FADER4;
      OSCfader[0] = inputValue.toFloat();
    }
    // Get value for P-Speed
    else if (request->hasParam(PARAM_FADER5)) {
      OSCpage = 2;
      inputValue = request->getParam(PARAM_FADER5)->value();
      inputMessage = PARAM_FADER5;
      OSCfader[0] = inputValue.toFloat();
    }
    // Get value for I-Speed
    else if (request->hasParam(PARAM_FADER6)) {
      OSCpage = 2;
      inputValue = request->getParam(PARAM_FADER6)->value();
      inputMessage = PARAM_FADER6;
      OSCfader[0] = inputValue.toFloat();
    }
    else {
      inputValue = "No message sent";
    }
    Serial.println(inputMessage+'='+inputValue);
    request->send(200, "text/text", "");
  });

  server.on("/web/jsData.html", handleJSData);  
  server.on("/web/joystick.html",handleJoystickHtml);
  server.on("/web/virtualjoystick.js",handleVirtualJoystickJS);
  server.on("/web/function1.html",handleFunktion1);
  server.on("/web/function2.html",handleFunktion2);
  server.on("/web/function3.html",handleFunktion3);
  server.on("/web", handleJoystickHtml);

  server.onNotFound (notFound);    // when a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  //AsyncElegantOTA.begin(&server);   // adding OTA via web-upload with http://192.168.4.1/update
  server.begin();                           // actually start the server

  initTimers();

  // default neutral values
  OSCfader[0] = 0.5;
  OSCfader[1] = 0.5;
  OSCfader[2] = 0.5;
  OSCfader[3] = 0.5;

  digitalWrite(PIN_ENABLE_MOTORS, LOW);
  for (uint8_t k = 0; k < 5; k++) {
    setMotorSpeedM1(5);
    setMotorSpeedM2(5);
    ledcWrite(6, SERVO_AUX_NEUTRO + 250);
    delay(200);
    setMotorSpeedM1(-5);
    setMotorSpeedM2(-5);
    ledcWrite(6, SERVO_AUX_NEUTRO - 250);
    delay(200);
  }
  ledcWrite(6, SERVO_AUX_NEUTRO);
}

void processOSCMsg() {
  if (OSCpage == 1) {
    if (modifing_control_parameters)  // We came from the settings screen
    {
      OSCfader[0] = 0.5; // default neutral values
      OSCfader[1] = 0.5; // default neutral values
      OSCtoggle[0] = 0;  // Normal mode
      mode = 0;
      modifing_control_parameters = false;
    }

    if (OSCmove_mode) {
      Serial.print("M ");
      Serial.print(OSCmove_speed);
      Serial.print(" ");
      Serial.print(OSCmove_steps1);
      Serial.print(",");
      Serial.println(OSCmove_steps2);
      positionControlMode = true;
      OSCmove_mode = false;
      target_steps1 = steps1 + OSCmove_steps1;
      target_steps2 = steps2 + OSCmove_steps2;
    } else {
      positionControlMode = false;
      throttle = (OSCfader[0] - 0.5) * max_throttle;
      // We add some exponential on steering to smooth the center band
      steering = OSCfader[1] - 0.5;
      if (steering > 0)
        steering = (steering * steering + 0.5 * steering) * max_steering;    
      else
        steering = (-steering * steering + 0.5 * steering) * max_steering;    
    }

    if ((mode == 0) && (OSCtoggle[0])) {
      // Change to PRO mode
      max_throttle = MAX_THROTTLE_PRO;
      max_steering = MAX_STEERING_PRO;
      max_target_angle = MAX_TARGET_ANGLE_PRO;
      mode = 1;
    }
    if ((mode == 1) && (OSCtoggle[0] == 0)) {
      // Change to NORMAL mode
      max_throttle = MAX_THROTTLE;
      max_steering = MAX_STEERING;
      max_target_angle = MAX_TARGET_ANGLE;
      mode = 0;
    }
  } else if (OSCpage == 2) { // OSC page 2
    if (!modifing_control_parameters) {
      for (uint8_t i = 0; i < 4; i++)
        OSCfader[i] = 0.5;
      OSCtoggle[0] = 0;

      modifing_control_parameters = true;
      //OSC_MsgSend("$P2", 4);
    }
    // User could adjust KP, KD, KP_THROTTLE and KI_THROTTLE (fadder3,4,5,6)
    // Now we need to adjust all the parameters all the times because we dont know what parameter has been moved
    Kp_user = KP * 2 * OSCfader[0];
    Kd_user = KD * 2 * OSCfader[1];
    Kp_thr_user = KP_THROTTLE * 2 * OSCfader[2];
    Ki_thr_user = KI_THROTTLE * 2 * OSCfader[3];
    // Send a special telemetry message with the new parameters
    char auxS[50];
    sprintf(auxS, "$tP,%d,%d,%d,%d", int(Kp_user * 1000), int(Kd_user * 1000), int(Kp_thr_user * 1000), int(Ki_thr_user * 1000));
    //OSC_MsgSend(auxS, 50);


    // Calibration mode??
    if (OSCpush[2] == 1) {
      Serial.print("Calibration MODE ");
      angle_offset = angle_adjusted_filtered;
      Serial.println(angle_offset);
    }

    // Kill robot => Sleep
    while (OSCtoggle[0] == 1) {
      //Reset external parameters
      PID_errorSum = 0;
      timer_old = millis();
      setMotorSpeedM1(0);
      setMotorSpeedM2(0);
      digitalWrite(PIN_ENABLE_MOTORS, HIGH);  // Disable motors
    }
  }
}


void loop() {

  //task A
    if (taskA.previous == 0 || (millis() - taskA.previous > taskA.rate)) {
        taskA.previous = millis(); 
        // 300 ms
        if (fromWeb) {
          fromWeb == 0;
          if (OSCpush[0]) {
            OSCpush[0]=0;
          }
        }
    }

  if (OSCnewMessage) {
    OSCnewMessage = 0;
    processOSCMsg();
  }

  timer_value = micros();

  if (MPU6050_newData()) {
    
    MPU6050_read_3axis();
    
    dt = (timer_value - timer_old) * 0.000001; // dt in seconds
    //Serial.println(timer_value - timer_old);
    timer_old = timer_value;

    angle_adjusted_Old = angle_adjusted;
    // Get new orientation angle from IMU (MPU6050)
    float MPU_sensor_angle = MPU6050_getAngle(dt);
    angle_adjusted = MPU_sensor_angle + angle_offset;
    if ((MPU_sensor_angle > -15) && (MPU_sensor_angle < 15))
      angle_adjusted_filtered = angle_adjusted_filtered * 0.99 + MPU_sensor_angle * 0.01;


    // We calculate the estimated robot speed:
    // Estimated_Speed = angular_velocity_of_stepper_motors(combined) - angular_velocity_of_robot(angle measured by IMU)
    actual_robot_speed = (speed_M1 + speed_M2) / 2; // Positive: forward

    int16_t angular_velocity = (angle_adjusted - angle_adjusted_Old) * 25.0; // 25 is an empirical extracted factor to adjust for real units
    int16_t estimated_speed = -actual_robot_speed + angular_velocity;
    estimated_speed_filtered = estimated_speed_filtered * 0.9 + (float) estimated_speed * 0.1; // low pass filter on estimated speed


    if (positionControlMode) {
      // POSITION CONTROL. INPUT: Target steps for each motor. Output: motors speed
      motor1_control = positionPDControl(steps1, target_steps1, Kp_position, Kd_position, speed_M1);
      motor2_control = positionPDControl(steps2, target_steps2, Kp_position, Kd_position, speed_M2);

      // Convert from motor position control to throttle / steering commands
      throttle = (motor1_control + motor2_control) / 2;
      throttle = constrain(throttle, -190, 190);
      steering = motor2_control - motor1_control;
      steering = constrain(steering, -50, 50);
    }

    // ROBOT SPEED CONTROL: This is a PI controller.
    //    input:user throttle(robot speed), variable: estimated robot speed, output: target robot angle to get the desired speed
    target_angle = speedPIControl(dt, estimated_speed_filtered, throttle, Kp_thr, Ki_thr);
    target_angle = constrain(target_angle, -max_target_angle, max_target_angle); // limited output

    // Stability control (100Hz loop): This is a PD controller.
    //    input: robot target angle(from SPEED CONTROL), variable: robot angle, output: Motor speed
    //    We integrate the output (sumatory), so the output is really the motor acceleration, not motor speed.
    control_output += stabilityPDControl(dt, angle_adjusted, target_angle, Kp, Kd);
    control_output = constrain(control_output, -MAX_CONTROL_OUTPUT,  MAX_CONTROL_OUTPUT); // Limit max output from control

    // The steering part from the user is injected directly to the output
    motor1 = control_output + steering;
    motor2 = control_output - steering;

    // Limit max speed (control output)
    motor1 = constrain(motor1, -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT);
    motor2 = constrain(motor2, -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT);

    int angle_ready;
    if (OSCpush[0])     // If we press the SERVO button we start to move
      angle_ready = 82;
    else
      angle_ready = 74;  // Default angle
    if ((angle_adjusted < angle_ready) && (angle_adjusted > -angle_ready)) // Is robot ready (upright?)
        {
      // NORMAL MODE
      digitalWrite(PIN_ENABLE_MOTORS, LOW);  // Motors enable
      // NOW we send the commands to the motors
      setMotorSpeedM1(motor1);
      setMotorSpeedM2(motor2);
    } else   // Robot not ready (flat), angle > angle_ready => ROBOT OFF
    {
      digitalWrite(PIN_ENABLE_MOTORS, HIGH);  // Disable motors
      setMotorSpeedM1(0);
      setMotorSpeedM2(0);
      PID_errorSum = 0;  // Reset PID I term
      Kp = KP_RAISEUP;   // CONTROL GAINS FOR RAISE UP
      Kd = KD_RAISEUP;
      Kp_thr = KP_THROTTLE_RAISEUP;
      Ki_thr = KI_THROTTLE_RAISEUP;
      // RESET steps
      steps1 = 0;
      steps2 = 0;
      positionControlMode = false;
      OSCmove_mode = false;
      throttle = 0;
      steering = 0;
    }
    
    // Push1 Move servo arm
    if (OSCpush[0]) {
      if (angle_adjusted > -40)
        ledcWrite(6, SERVO_MAX_PULSEWIDTH);
      else
        ledcWrite(6, SERVO_MIN_PULSEWIDTH);
    } else
      ledcWrite(6, SERVO_AUX_NEUTRO);

    // Servo2
    //ledcWrite(6, SERVO2_NEUTRO + (OSCfader[2] - 0.5) * SERVO2_RANGE);

    // Normal condition?
    if ((angle_adjusted < 56) && (angle_adjusted > -56)) {
      Kp = Kp_user;            // Default user control gains
      Kd = Kd_user;
      Kp_thr = Kp_thr_user;
      Ki_thr = Ki_thr_user;
    } else // We are in the raise up procedure => we use special control parameters
    {
      Kp = KP_RAISEUP;         // CONTROL GAINS FOR RAISE UP
      Kd = KD_RAISEUP;
      Kp_thr = KP_THROTTLE_RAISEUP;
      Ki_thr = KI_THROTTLE_RAISEUP;
    }

  } // End of new IMU data

}



