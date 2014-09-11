// taken from: http://blog.crondesign.com/2009/09/scrollfix-remember-scroll-position-when.html

////////////////////////////////
// fixscroll.js:
// call loadP and unloadP when body loads/unloads and scroll position will not move
function getScrollXY() {
    var x = 0, y = 0;
    if( typeof( window.pageYOffset ) == 'number' ) {
        // Netscape
        x = window.pageXOffset;
        y = window.pageYOffset;
    } else if( document.body && ( document.body.scrollLeft || document.body.scrollTop ) ) {
        // DOM
        x = document.body.scrollLeft;
        y = document.body.scrollTop;
    } else if( document.documentElement && ( document.documentElement.scrollLeft || document.documentElement.scrollTop ) ) {
        // IE6 standards compliant mode
        x = document.documentElement.scrollLeft;
        y = document.documentElement.scrollTop;
    }
    return [x, y];
}
           
function setScrollXY(x, y) {
    window.scrollTo(x, y);
}
function createCookie(name,value,days) {
    if (days) {
	var date = new Date();
	date.setTime(date.getTime()+(days*24*60*60*1000));
	var expires = "; expires="+date.toGMTString();
	}
    else var expires = "";
    document.cookie = name+"="+value+expires+"; path=/";
}

function readCookie(name) {
    var nameEQ = name + "=";
    var ca = document.cookie.split(';');
    for(var i=0;i < ca.length;i++) {
	var c = ca[i];
	while (c.charAt(0)==' ') c = c.substring(1,c.length);
	if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
	}
    return null;
}
function loadP(pageref){
    x=readCookie(pageref+'x');
    y=readCookie(pageref+'y');
    setScrollXY(x,y)
}
function unloadP(pageref){
    s=getScrollXY()
    createCookie(pageref+'x',s[0],0.1);
    createCookie(pageref+'y',s[1],0.1);
}
