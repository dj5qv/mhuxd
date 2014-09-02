

<?cs call:sectionheader("Summary", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <tr>
	      <td class="titlesettingscell" align="right">Version:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left"><?cs var:mhuxd.run.program.name ?> <?cs var:mhuxd.run.program.version ?></td>
	    </tr>

	    <tr>
	      <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	    </tr>

	    <tr>
	      <td class="titlesettingscell" align="right">Hostname:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left"><?cs var:mhuxd.run.hostname ?></td>
	    </tr>

	    <tr>
	      <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	    </tr>

	    <tr>
	      <td class="titlesettingscell" align="right">Process ID:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left"><?cs var:mhuxd.run.pid ?></td>
	    </tr>

	    <tr>
	      <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	    </tr>

	    <tr>
	      <td class="titlesettingscell" align="right">Log File:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left"><?cs var:mhuxd.run.logfile ?></td>
	    </tr>

	    <tr>
	      <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	    </tr>

	    <tr>
	      <td class="titlesettingscell" align="right">Loglevel:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left"><?cs var:mhuxd.daemon.loglevel ?></td>
	    </tr>

	    <tr>
	      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
	      <td><img src="/static/dot.gif" alt="" height="1" width="1"></td>
	      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
	    </tr>

	  </tbody>
	</table>
      </td>
    </tr>
  </tbody>
</table>
<br>
<br>

<?cs call:sectionheader("Keyer List", "keyer_list_help") ?>
&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="3" cellspacing="0" width="100%">
  <tbody>

    <tr>
      <td class="titlelistcell">Name</td>
      <td class="titlelistcell">Serial</td>
      <td class="titlelistcell">Firmware</td>
      <td class="titlelistcell">Status</td>
    </tr>

    <?cs each:item = mhuxd.run.keyer ?>
    <tr class="contentlistrow2">
      <td class="contentlistcell"><?cs var:item.info.name ?></td>
      <td class="contentlistcell"><?cs var:name(item) ?></td>
      <td class="contentlistcell"><?cs var:item.info.ver_fw_major ?>.<?cs var:item.info.ver_fw_minor ?></td>
      <td class="contentlistcell">  <?cs call:keyer_status_icon(name(item)) ?>  <?cs var:item.info.status ?></td>
    </tr>
    <?cs /each ?>

  </tbody>
</table>

<?cs if:subcount(mhuxd.keyer) == 0 ?>
<br> <div class="novalues">No keyers found.</div>
<?cs /if ?>
