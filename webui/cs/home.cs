<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<?cs include:"macros.cs" ?>
<html>
  <head>
    <title><?cs var:mhuxd.webui.title ?></title>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <link href="/static/mhuxd.css" rel="stylesheet" type="text/css">
    <script src="/static/scrollfix.js" type="text/javascript"></script>
  </head>
  <body 
     topmargin="0" 
     leftmargin="0" 
     marginheight="0" 
     marginwidth="0"  
     onunload="unloadP('UniquePageNameHereScroll')" 
     onload="loadP('UniquePageNameHereScroll')" 
     >

    <!-- HEADER -->
    <div id="header">
    <?cs include:"pageheader.cs"  ?>
    <?cs include:"sectionheader.cs" ?>
    </div>

    <!-- LEFT MENU -->
    <div id="left">
      <?cs include:"left_menu.cs" ?>
    </div>


    <!-- MAIN -->
    <div id="main">
      &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
      <div id="small-main-header" class="smalltextgray">
	<?cs if:mhuxd.webui.session.unit ?>
	<td class="smalltextgray"><?cs var:mhuxd.webui.tabs[mhuxd.webui.session.unit].display ?></td>
	<?cs else ?>
	<td class="smalltextgray"><?cs var:mhuxd.webui.tabs[mhuxd.webui.session.page].display ?></td>
	<?cs /if ?>
	<td class="smalltextgray">&nbsp;&nbsp;&#187;&nbsp;&nbsp;</td>
	<td class="smalltextgray"><?cs var:mhuxd.webui.menus[mhuxd.webui.session.page][mhuxd.webui.session.menu].display ?></td>
      </div>


      <h1><?cs var:mhuxd.webui.menus[mhuxd.webui.session.page][mhuxd.webui.session.menu].title ?></h1>
      <form name="MhuxdConfigForm" method="POST" 
	    action="?page=<?cs var:mhuxd.webui.session.page ?>&unit=<?cs var:mhuxd.webui.session.unit ?>&menu=<?cs var:mhuxd.webui.session.menu ?>&edit=1">

	<?cs if:mhuxd.webui.session.unit ?>
	<?cs call:hidden("metaset.mhuxd.webui.meta.keyer."+mhuxd.webui.session.unit+".type", mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>
	<?cs /if ?>

	<!-- ERROR MESSAGE -->
	<?cs if:mhuxd.webui.notify.error ?>
	<img src="/static/dot.gif" width="1" height="4" border="0"><br>
	<table border="0" cellspacing="0" cellpadding="1">
	  <tr>
	    <td><table class="error" border="0" cellspacing="0" cellpadding="3">
		<tr>
		  <td class="errorpic"><img src="/static/stop.gif"></td>
		  <td><font class="msgtype"><div class="msgtype"><strong> Error: </strong></div></font></td>
		  <td>
		    <font class="msgtext">
		      <div class="msgtext">
			<?cs var:mhuxd.webui.notify.error ?>
		      </div>
		  </font></td>
		</tr>
	      </table>
	    </td>
	  </tr>
	</table>
	<img src="/static/dot.gif" width="1" height="4" border="0"><br>
	<br>
	<?cs /if ?>

	<!-- WARNING MESSAGE -->
	<?cs if:mhuxd.webui.notify.warning ?>
	<img src="/static/dot.gif" width="1" height="4" border="0"><br>
	<table border="0" cellspacing="0" cellpadding="1">
	  <tr>
	    <td><table class="warning" border="0" cellspacing="0" cellpadding="3">
		<tr>
		  <td class="warningpic"><img src="/static/warning.gif"></td>
		  <td><font class="msgtype"><div class="msgtype"><strong> Error: </strong></div></font></td>
		  <td>
		    <font class="msgtext">
		      <div class="msgtext">
			<?cs var:mhuxd.webui.notify.warning ?>
		      </div>
		  </font></td>
		</tr>
	      </table>
	    </td>
	  </tr>
	</table>
	<img src="/static/dot.gif" width="1" height="4" border="0"><br>
	<br>
	<?cs /if ?>

	<!-- INFO MESSAGE -->
	<?cs if:mhuxd.webui.notify.info ?>
	<img src="/static/dot.gif" width="1" height="4" border="0"><br>
	<table border="0" cellspacing="0" cellpadding="1">
	  <tr>
	    <td><table class="info" border="0" cellspacing="0" cellpadding="3">
		<tr>
		  <td class="infopic"><img src="/static/note.gif"></td>
		  <td><font class="msgtype"><div class="msgtype"><strong> Info: </strong></div></font></td>
		  <td>
		    <font class="msgtext">
		      <div class="msgtext">
			<?cs var:mhuxd.webui.notify.info ?>
		      </div>
		  </font></td>
		</tr>
	      </table>
	    </td>
	  </tr>
	</table>
	<img src="/static/dot.gif" width="1" height="4" border="0"><br>
	<br>
	<?cs /if ?>


	<div id="mainarea">
	  <?cs include:mhuxd.webui.menus[mhuxd.webui.session.page][mhuxd.webui.session.menu].include ?>
	</div>


      </form>





    </div>

  </body>
</html>
