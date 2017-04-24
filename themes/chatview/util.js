function initPsiTheme() {
    var server = window.srvUtil;
    var loader = window.srvLoader;
    var session = window.srvSession;
    server.console("Util is initilizing");
    var htmlSource = document.createElement("div"); //manages appendHtml
    var async = (typeof QWebChannel != 'undefined');
    var usersMap = {};

    var chat =  {
        async : async,
        console : server.console,
        server : server,

        util: {
            console : server.console,
            showCriticalError : function(text) {
                var e=document.body || document.documentElement.appendChild(document.createElement("body"));
                var er = e.appendChild(document.createElement("div"))
                er.style.cssText = "background-color:red;color:white;border:1px solid black;padding:1em;margin:1em;font-weight:bold";
                er.innerHTML = chat.util.escapeHtml(text).replace(/\n/, "<br/>");
            },

            // just for debug
            escapeHtml : function(html) {
                html += ""; //hack
                return html.split("&").join("&amp;").split( "<").join("&lt;").split(">").join("&gt;");
            },

            // just for debug
            props : function(e, rec) {
                var ret='';
                for (var i in e) {
                    var gotValue = true;
                    var val = null;
                    try {
                        val = e[i];
                    } catch(err) {
                        val = err.toString();
                        gotValue = false;
                    }
                    if (gotValue) {
                        if (val instanceof Object && rec && val.constructor != Date) {
                            ret+=i+" = "+val.constructor.name+"{"+chat.util.props(val, rec)+"}\n";
                        } else {
                            if (val instanceof Function) {
                                ret+=i+" = Function: "+i+"\n";
                            } else {
                                ret+=i+" = "+(val === null?"null\n":val.constructor.name+"(\""+val+"\")\n");
                            }
                        }
                    } else {
                        ret+=i+" = [CAN'T GET VALUE: "+val+"]\n";
                    }
                }
                return ret;
            },

            _remoteCallEval : function(func, args, cb) {
                function ecb(val) { val = eval("[" + val + "][0]"); cb(val); }

                if (chat.async) {
                    args.push(ecb)
                    func.apply(this, args)
                } else {
                    var val = func.apply(this, args);
                    ecb(val);
                }
            },

            _remoteCall : function(func, args, cb) {
                if (chat.async) {
                    args.push(cb)
                    func.apply(this, args)
                } else {
                    var val = func.apply(this, args);
                    cb(val);
                }
            },

            psiOption : function(option, cb) { chat.util._remoteCallEval(server.psiOption, [option], cb); },
            colorOption : function(option, cb) { chat.util._remoteCallEval(server.colorOption, [option], cb); },
            getFont : function(cb) { chat.util._remoteCallEval(session.getFont, [], cb); },
            getPaletteColor : function(name, cb) { chat.util._remoteCall(session.getPaletteColor, [name], cb); },

            // replaces <icon name="icon_name" text="icon_text" />
            // with <img src="/psiicon/icon_name" title="icon_text" />
            icon2img : function (obj) {
               var img = document.createElement('img');
               img.src = "/psiicon/" + obj.getAttribute("name");
               img.title = obj.getAttribute("text");
               obj.parentNode.replaceChild(img, obj);
            },

            // replaces all occurrence of <icon> by function above
            replaceIcons : function(el) {
                var els = el.getElementsByTagName("icon");
                while (els.length) {
                    chat.util.icon2img(els[0]);
                }
            },

            updateObject : function(object, update) {
                for (var i in update) {
                    object[i] = update[i]
                }
            },

            findStyleSheet : function (sheet, selector) {
                for (var i=0; i<sheet.cssRules.length; i++) {
                    if (sheet.cssRules[i].selectorText == selector)
                        return sheet.cssRules[i];
                }
                return false;
            },

            createHtmlNode : function(html, context) {
                var range = document.createRange();
                range.selectNode(context || document.body);
                return range.createContextualFragment(html);
            },

            appendHtml : function(dest, html) {
                htmlSource.innerHTML = html;
                chat.util.replaceIcons(htmlSource);
                while (htmlSource.firstChild) dest.appendChild(htmlSource.firstChild);
            },

            siblingHtml : function(dest, html) {
                htmlSource.innerHTML = html;
                chat.util.replaceIcons(htmlSource);
                while (htmlSource.firstChild) dest.parentNode.insertBefore(htmlSource.firstChild, dest);
            },

            ensureDeleted : function(id) {
                if (id) {
                    var el = document.getElementById(id);
                    if (el) {
                        el.parentNode.removeChild(el);
                    }
                }
            },

            loadXML : function(path, callback) {
                function cb(text){
                    if (!text) {
                        throw new Error("File " + path + " is empty. can't parse xml");
                    }
                    var xml;
                    try {
                        xml = new DOMParser().parseFromString(text, "text/xml");
                    } catch (e) {
                        server.console("failed to parse xml from file " + path);
                        throw e;
                    }
                    callback(xml);
                }
                if (chat.async) {
                    server.console("loading xml async: " + path);
                    loader.getFileContents(path, cb);
                } else {
                    server.console("loading xml sync: " + path);
                    cb(loader.getFileContents(path));
                }
            },

            dateFormat : function(val, format) {
                return (new chat.DateTimeFormatter(format)).format(val);
            },

            avatarForNick : function(nick) {
                var u = usersMap[nick];
                return u && u.avatar;
            },

            nickColor : function(nick) {
                var u = usersMap[nick];
                return u && u.nickcolor;
            },

            replaceableMessage : function(msgId, text) {
                // if we have an id then this is a replacable message.
                // next weird logic is as follows:
                //   - wrapping to some element may break elements flow
                //   - using well know elements may break styles
                //   - setting just starting mark is useless (we will never find correct end)
                return "<psims mid=\"" + msgId + "\"></psims>" + text + "<psime mid=\"" + msgId + "\"></psime>";
            },

            replaceMessage : function(parentEl, msgId, newId, text) {
                var se =parentEl.querySelector("psims[mid='"+msgId+"']");
                var ee =parentEl.querySelector("psime[mid='"+msgId+"']");
                chat.console("Replace: start: " + (se? "found, ":"not found, ") +
                             "end: " + (ee? "found, ":"not found, ") +
                             "parent match: " + ((se && ee && se.parentNode === ee.parentNode)?"yes":"no"));
                if (se && ee && se.parentNode === ee.parentNode) {
                    while (se.nextSibling !== ee) {
                        se.parentNode.removeChild(se.nextSibling);
                    }
                    var node = chat.util.createHtmlNode(chat.util.replaceableMessage(newId, text + "<img src=\"/psiicon/psi/action_templates_edit\">"));
                    ee.parentNode.insertBefore(node, ee);
                }
            }
        },

        WindowScroller : function(animate) {
            var o=this, state, timerId
            var ignoreNextScroll = false;
            o.animate = animate;
            o.atBottom = true; //just a state of aspiration

            var animationStep = function() {
                timerId = null;
                var before = document.body.clientHeight - (window.innerHeight+window.pageYOffset);
                var step = before;
                if (o.animate) {
                    step = step>200?200:(step<8?step:Math.floor(step/1.7));
                }
                ignoreNextScroll = true;
                window.scrollTo(0, document.body.clientHeight - window.innerHeight - before + step);
                if (before>0) {
                    timerId = setTimeout(animationStep, 70); //next step in 250ms even if we are already at bottom (control shot)
                }
            }

            var startAnimation = function() {
                if (timerId) return;
                if (document.body.clientHeight > window.innerHeight) { //if we have what to scroll
                    timerId = setTimeout(animationStep, 0);
                }
            }

            var stopAnimation = function() {
                if (timerId) {
                    clearTimeout(timerId);
                    timerId = null;
                }
            }

            //timeout to be sure content rerendered correctly
            window.addEventListener("resize", function() {setTimeout(function(){
                if (o.atBottom) { //immediatelly scroll to bottom if we wish it
                    window.scrollTo(0, document.body.clientHeight - window.innerHeight);
                }
            }, 0);}, false);

            //let's consider scroll may happen only by user action
            window.addEventListener("scroll", function(){
                if (ignoreNextScroll) {
                    ignoreNextScroll = false;
                    return;
                }
                stopAnimation();
                o.atBottom = document.body.clientHeight == (window.innerHeight+window.pageYOffset);
            }, false);

            //EXTERNAL API
            // checks current state of scroll and wish and activates necessary actions
            o.invalidate = function() {
                if (o.atBottom) {
                    startAnimation();
                }
            }

            o.force = function() {
                o.atBottom = true;
                o.invalidate();
            }
        },

        DateTimeFormatter : function(formatStr) {
            function convertToTr35(format)
            {
                var ret=""
                var i = 0;
                var m = {M: "mm", H: "HH", S: "ss", c: "EEEE', 'MMMM' 'd', 'yyyy' 'G",
                         A: "EEEE", I: "hh", p: "a", Y: "yyyy"}; // if you want me, report it.

                var txtAcc = "";
                while (i < format.length) {
                    var c;
                    if (format[i] === "'" ||
                            (format[i] === "%" && i < (format.length - 1) && (c = m[format[i+1]])))
                    {
                        if (txtAcc) {
                            ret += "'" + txtAcc + "'";
                            txtAcc = "";
                        }
                        if (format[i] === "'") {
                            ret += "''";
                        } else {
                            ret += c;
                            i++;
                        }
                    } else {
                        txtAcc += format[i];
                    }
                    i++;
                }
                if (txtAcc) {
                    ret += "'" + txtAcc + "'";
                    txtAcc = "";
                }
                return ret;
            }

            function convertToMoment(format) {
                var inTxt = false;
                var i;
                var m = {j:"h"}; // sadly "j" is not supported
                var ret = "";
                for (i = 0; i < format.length; i++) {
                    if (format[i] == "'") {
                        ret += (inTxt? ']' : '[');
                        inTxt = !inTxt;
                    } else {
                        var c;
                        if (!inTxt && (c = m[format[i]])) {
                            ret += c;
                        } else {
                            ret += format[i];
                        }
                    }
                }
                if (inTxt) {
                    ret += "]";
                }

                ret = ret.replace("EEEE", "dddd");
                ret = ret.replace("EEE", "ddd");

                return ret;
            }

            formatStr = formatStr || "j:mm";
            if (formatStr.indexOf('%') !== -1) {
                formatStr = convertToTr35(formatStr);
            }

            formatStr = convertToMoment(formatStr);

            this.format = function(val) {
                if (val instanceof String) {
                    val = Date.parse(val);
                }
                return moment(val).format(formatStr); // FIXME we could speedup it by keeping fomatter instances
            }
        },

        receiveObject : function(data) {
            if (data.type == "message") {
                if (data.mtype == "join") {
                    usersMap[data.sender] = {avatar:data.avatar, nickcolor:data.nickcolor};
                    if (data.nopartjoin) return;
                } else if (data.mtype == "part") {
                    delete usersMap[data.sender];
                    if (data.nopartjoin) return;
                } else if (data.mtype == "newnick") {
                    usersMap[data.newnick] = usersMap[data.sender];
                    delete usersMap[data.sender];
                }
            } else if (data.type == "avatar") {
                usersMap[data.sender].avatar = data.avatar;
            }

            chat.adapter.receiveObject(data)
        }
    }

    try {
        chat.adapter = window.psiThemeAdapter(chat);
    } catch(e) {
        server.console("Failed to initialize adapter:" + e + "(Line:" + e.line + ")");
        chat.adapter = {
            receiveObject : function(data) {
                chat.util.showCriticalError("Adapter is not loaded. output impossible!\n\nData was:" + chat.util.props(data));
            }
        }
    }

    //window.srvUtil = null; // don't! we need it in adapter
    window.psiThemeAdapter = null;
    window.psiim = chat;

    server.console("Util successfully initialized");
    return chat;
};
