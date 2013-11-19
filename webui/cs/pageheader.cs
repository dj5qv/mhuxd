
<!-- PAGEHEADER -->

<table class="pageHeaderTable" border="0" cellpadding="0" cellspacing="0" width="100%">
  <tbody><tr>
      <td valign="top">
	<table border="0" cellpadding="0" cellspacing="0">
	  <tbody>
	    <tr>
	      <td align="left" height="50" width="25">&nbsp;</td>
	      <!--
		  <td valign="middle"><img src="/static/mhuxd.png" alt="mhuxd" border="0" height="35" width="350"></td>
		  -->
	      <td class="pageHeaderText" valign="middle"><i>m</i>huxd Device Router</td>
	      <td align="left" height="50" width="25">&nbsp;</td>

	    </tr>
	  </tbody>
	</table>
      </td>
      <td rowspan="2" align="right" valign="top">
	<table border="0" cellpadding="4" cellspacing="0">
	  <tbody>
	    <tr>
	      <td class="smallTextWhite" align="right" nowrap="nowrap">Hostname: <?cs var:mhuxd.run.hostname ?>
		|<?cs var:mhuxd.run.program.version ?>|
	      </td>
	     
	    </tr>
	    <tr>
	      <td class="regularTextWhiteBold" align="right" nowrap="nowrap">
		<a href="http://mhuxd.dj5qv.de/doc/" class="bannerlink" target="_new">Help</a> 
	    </tr>
	  </tbody>
	</table>
      </td>
    </tr>

    <!-- NAVTAB -->

    <tr>
      <td valign="bottom">
	<table border="0" cellpadding="0" cellspacing="0">
	  <tbody>
	    <tr>
	      <td class="navButtonSpacer"><img src="/static/dot.gif" alt="" height="5" width="5"></td>

	      <?cs each:item = mhuxd.webui.tabs ?>

	      <?cs if:mhuxd.webui.session.page == item.page && mhuxd.webui.session.unit == item.unit ?>

	      <td class="navButtonCornerSelected" valign="top"><img src="/static/navtab2-sel-left.gif" alt="" height="5" width="5"></td>

	      <?cs if:item.unit != "0" ?>
	      <td class="navButtonSelected"><?cs call:keyer_status_icon(item.unit) ?></td>
	      <?cs /if ?>

	      <td class="navButtonSelected">
		<a href="<?cs var:mhuxd.webui.base_url ?>?page=<?cs var:item.page ?>&unit=<?cs var:item.unit ?>&menu=<?cs var:mhuxd.webui.defaults[item.page].default_menu ?>" 
		   class="tablink2"><?cs var:item.display ?></a></td>
	      <td class="navButtonCornerSelected" valign="top"><img src="/static/navtab2-sel-right.gif" alt="" height="5" width="5"></td>
	      <td class="navButtonSpacer"><img src="/static/dot.gif" alt="" height="5" width="5"></td>

	      <?cs else ?>

	      <td class="navButtonCorner" valign="top"><img src="/static/navtab2-left.gif" alt="" height="5" width="5"></td>

	      <?cs if:item.unit != "0" ?>
	      <td class="navButton"><?cs call:keyer_status_icon(item.unit) ?></td>
	      <?cs /if ?>

	      <td class="navButton">
		<a href="<?cs var:mhuxd.webui.base_url ?>?page=<?cs var:item.page ?>&unit=<?cs var:item.unit ?>&menu=<?cs var:mhuxd.webui.defaults[item.page].default_menu ?>" 
		   class="tablink"><?cs var:item.display ?></a></td>
	      <td class="navButtonCorner" valign="top"><img src="/static/navtab2-right.gif" alt="" height="5" width="5"></td>
	      <td class="navButtonSpacer"><img src="/static/dot.gif" alt="" height="5" width="5"></td>

	      <?cs /if ?>

	      <?cs /each ?>

	    </tr>
	  </tbody>
	</table>
      </td>
    </tr>
    <tr>
      <td class="bottomnavspacer" colspan="2"><img src="/static/dot.gif" alt="" height="5" width="1"></td>
    </tr>
  </tbody>
</table>
