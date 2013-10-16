<?cs include:"macros.cs" ?>
<html><head>
<title><?cs var:mhuxd.webui.title ?></title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<link href="/static/mhuxd.css" rel="stylesheet" type="text/css">

</head>
  <body topmargin="0" leftmargin="0" marginheight="0" marginwidth="0">
    <?cs include:"pageheader.cs"  ?>
    <?cs include:"sectionheader.cs" ?>
    <table border="0" cellpadding="0" cellspacing="0" width="100%">
      <tbody>
	<tr>
	  <?cs include:"left_menu.cs" ?>
	  <!-- fooooo -->
	  <td valign="top">
	      <table border="0" cellpadding="0" cellspacing="0" width="100%">
		<tbody>
		  <tr>
		  <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>
		  <td align="" valign="top" width="100%">

		    <!-- SMALL HEADER -->
		    <table border="0" cellspacing="0" cellpadding="0">
		      <tr>
			<td><img src="/static/dot.gif" width="1" height="5" alt=""></td>
		      </tr>
		      <tr>
			<?cs if:mhuxd.webui.session.unit ?>
			<td class="smalltextgray"><?cs var:mhuxd.webui.tabs[mhuxd.webui.session.unit].display ?></td>
			<?cs else ?>
			<td class="smalltextgray"><?cs var:mhuxd.webui.tabs[mhuxd.webui.session.page].display ?></td>
			<?cs /if ?>

			<td class="smalltextgray">&nbsp;&nbsp;&#187;&nbsp;&nbsp;</td>
			<td class="smalltextgray"><?cs var:mhuxd.webui.menus[mhuxd.webui.session.page][mhuxd.webui.session.menu].display ?></td>
		      </tr>
		    </table>

		    <h1><?cs var:mhuxd.webui.menus[mhuxd.webui.session.page][mhuxd.webui.session.menu].title ?></h1>
		    <form name="MhuxdConfigForm" method="POST" 
			  action="?page=<?cs var:mhuxd.webui.session.page ?>&unit=<?cs var:mhuxd.webui.session.unit ?>&menu=<?cs var:mhuxd.webui.session.menu ?>&edit=1">

		      <?cs if:mhuxd.webui.session.unit ?>
		      <?cs call:hidden("metaset.mhuxd.webui.meta.keyer."+mhuxd.webui.session.unit+".type", mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>
		      <?cs /if ?>

		      <?cs include:mhuxd.webui.menus[mhuxd.webui.session.page][mhuxd.webui.session.menu].include ?>

		      <!-- ERROR MESSAGE -->
		      <?cs if:mhuxd.webui.notify.error ?>
		      <br>

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
		      <?cs /if ?>
		    </form>
		  </td>
		  <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>
		</tr>
		<tr>
		  <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>
		  <td align="" valign="top" width="100%"></td>
		  <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>
		</tr>
	    </tbody></table>
	  </td>
	</tr>
      </tbody>
    </table>
  </body>
</html>
