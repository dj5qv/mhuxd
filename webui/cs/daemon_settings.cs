
<?cs call:sectionheader("Logging", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <tr>
	      <td class="titlesettingscell" align="right">Log Level:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>


	      <?cs if:mhuxd.webui.session.EditLogging ?>
	      <td class="contentsettingscell" align="left"> 
	      	<select name="set.mhuxd.daemon.loglevel">
		  <option <?cs if:mhuxd.daemon.loglevel=="CRIT"  ?>selected<?cs /if ?> value="CRIT">CRIT</option>
		  <option <?cs if:mhuxd.daemon.loglevel=="ERROR" ?>selected<?cs /if ?> value="ERROR">ERROR</option>
		  <option <?cs if:mhuxd.daemon.loglevel=="WARN"  ?>selected<?cs /if ?> value="WARN">WARN</option>
		  <option <?cs if:mhuxd.daemon.loglevel=="INFO"  ?>selected<?cs /if ?> value="INFO">INFO</option>
		  <option <?cs if:mhuxd.daemon.loglevel=="DEBUG0"?>selected<?cs /if ?> value="DEBUG0">DEBUG0</option>
		  <option <?cs if:mhuxd.daemon.loglevel=="DEBUG1"?>selected<?cs /if ?> value="DEBUG1">DEBUG1</option>
		</select>
	      </td>
	      <?cs else ?>
	      <td class="contentsettingscell" align="left"><?cs var:mhuxd.daemon.loglevel ?></td>
	      <?cs /if ?>
	    </tr>

<!--
	    <tr>
	      <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	    </tr>
-->

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

<img src="/static/dot.gif" height="4" border="0" width="1"><br>

<?cs if:mhuxd.webui.session.EditLogging ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="EditLogging" onclick="button_clicked=this.value;" value="Edit" type="submit">
<?cs /if ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>





