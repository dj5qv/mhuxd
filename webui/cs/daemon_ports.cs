
<?cs if:mhuxd.webui.session.CreatePort ?>


<?cs if:mhuxd.webui.session.set.mhuxd.connector.0.type ?>
<input type="hidden" name="set.mhuxd.connector.0.type" value="<?cs var:mhuxd.webui.session.set.mhuxd.connector.0.type ?>">
<?cs /if ?>
<?cs if:mhuxd.webui.session.set.mhuxd.connector.0.serial ?>
<input type="hidden" name="set.mhuxd.connector.0.serial" value="<?cs var:mhuxd.webui.session.set.mhuxd.connector.0.serial ?>">
<?cs /if ?>


<?cs call:sectionheader("Create  Port", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <!-- VSP PORT -->
	    <?cs if:mhuxd.webui.session.set.mhuxd.connector.0.type == "VSP" ?>
	    <tr>
	      <td class="titlesettingscell" align="right">Device Path:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left">
		/dev/mhuxd/ <input type="text" name="set.mhuxd.connector.0.devname" value="">
	      </td>
	    </tr>

	    <tr>
	      <td class="titlesettingscell" align="right">PTT via RTS</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left">
		<input type="checkbox" name="set.mhuxd.connector.0.ptt_rts" value="1">
	      </td>
	    </tr>

	    <tr>
	      <td class="titlesettingscell" align="right">PTT via DTR</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left">
		<input type="checkbox" name="set.mhuxd.connector.0.ptt_dtr" value="1">
	      </td>
	    </tr>
	    <?cs /if ?>

	    <!-- TCP PORT -->
	    <?cs if:mhuxd.webui.session.set.mhuxd.connector.0.type == "TCP" ?>

	    <tr>
	      <td class="titlesettingscell" align="right">Port Number:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell" align="left">
		<input type="text" name="set.mhuxd.connector.0.devname" value="">
	      </td>
	    </tr>


	    <tr>
	      <td class="titlesettingscell" align="right">Remote Accessible:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell">
		<input type="checkbox"
		       name="set.mhuxd.connector.0.remote_access"
		       value="1"
		       >
	      </td>
	    </tr>
	    <?cs /if ?>

	    <tr>
	      <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	    </tr>

	    <!-- DESTINATION -->
	    <tr>
	      <td class="titlesettingscell" align="right">Destination Channel:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>


	      <td class="contentsettingscell" align="left">
		<table>
		  <tr>
		    <td>
	      	      <select name="set.mhuxd.connector.0.channel">
			<?cs each:channel = mhuxd.run.keyer[mhuxd.webui.session.set.mhuxd.connector.0.serial].channels ?>
			<option value="<?cs var:name(channel) ?>"><?cs var:name(channel) ?></option>
			<?cs /each ?>
		      </select>
		    </td>
		  </tr>
		</table>
	      </td>
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


&nbsp;&nbsp;<br>


<img src="/static/dot.gif" height="4" border="0" width="1"><br>

<input name="SaveButton" onclick="button_clicked=this.value;" value="Create" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">



<?cs else ?>
<!-- BASE PAGE -->


<?cs call:sectionheader("Port List", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="3" cellspacing="0" width="100%">
  <tbody>
    <tr>
      <td class="titlelistcell">&nbsp;</td>
      <td class="titlelistcell">ID</td>
      <td class="titlelistcell">Type</td>
      <td class="titlelistcell">Port / Device</td>
      <td class="titlelistcell">Remote Access</td>
      <td class="titlelistcell">RTS-PTT</td>
      <td class="titlelistcell">DTR-PTT</td>
      <td class="titlelistcell">Destination Name</td>
      <td class="titlelistcell">Destination Serial</td>
      <td class="titlelistcell">Channel</td>
    </tr>

    <?cs each:item = mhuxd.connector ?>

    <tr class="contentlistrow2">
      <td class="radiolistcell2" width="19" align="center">
	<input type="checkbox" name="unset.mhuxd.connector.<?cs var:name(item) ?>" value="1" >
      </td>
      <td class="contentlistcell"><?cs var:name(item) ?></td>
      <td class="contentlistcell"><?cs var:item.type ?></td>
      <td class="contentlistcell"><?cs var:item.devname ?></td>

      <?cs if:item.type == "VSP" ?>
      <td class="contentlistcell">&nbsp</td>		

      <?cs if:item.ptt_rts == 1 ?>
      <td class="contentlistcell"><img src="/static/checkmark.gif" alt="true"></td>
      <?cs else ?>
      <td class="contentlistcell"><img src="/static/nocheckmark.gif" alt="false"></td>
      <?cs /if ?>

      <?cs if:item.ptt_dtr == 1 ?>
      <td class="contentlistcell"><img src="/static/checkmark.gif" alt="true"></td>
      <?cs else ?>
      <td class="contentlistcell"><img src="/static/nocheckmark.gif" alt="false"></td>
      <?cs /if ?>

      <?cs else ?>
      <?cs if:item.remote_access == 1 ?>
      <td class="contentlistcell"><img src="/static/checkmark.gif" alt="true"></td>
      <?cs else ?>
      <td class="contentlistcell"><img src="/static/nocheckmark.gif" alt="false"></td>
      <?cs /if ?>

      <td class="contentlistcell">&nbsp</td>		
      <td class="contentlistcell">&nbsp</td>		
      <?cs /if ?>

      <td class="contentlistcell"><?cs var:mhuxd.run.keyer[item.serial].info.name ?></td>
      <td class="contentlistcell"><?cs var:item.serial ?></td>
      <td class="contentlistcell"><?cs var:item.channel ?></td>
    </tr>

    <?cs /each ?>

  </tbody>
</table>

<?cs if:subcount(mhuxd.connector) == 0 ?>

<br> <div class="novalues">No ports configured.</div>

<?cs else ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<input name="RemoveButton" onclick="button_clicked=this.value;" value="Remove" type="submit">
<img src="/static/dot.gif" height="4" border="0" width="1"><br>

<?cs /if ?>

<br>

<?cs call:sectionheader("Add Port", "foobar_help") ?>


<br>
<table class="sectiontable" cellspacing="0" cellpadding="0">
  <tr>
    <td>
      <table class="sectiontableinner" cellspacing="0" cellpadding="0">
	<tr>
	  <td class="titlesettingscell" align="right">Port Type:</td>
	  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	  <td class="contentsettingscell" align="left">
	    <table>
	      <tr>
		<td><input type="radio" value="VSP" name="set.mhuxd.connector.0.type" 
			   <?cs if:mhuxd.webui.session.set.mhuxd.connector.0.type == "VSP" || 
				!mhuxd.webui.session.set.mhuxd.connector.0.type ?>
			   checked
			   <?cs /if ?>  ></td>
			   
		<td class="contentsettingscell">VSP Virtual Serial Port</td>
	      </tr>
	      <tr>
		<td><input type="radio" value="TCP" name="set.mhuxd.connector.0.type" 
			   <?cs if:mhuxd.webui.session.set.mhuxd.connector.0.type == "TCP" ?>checked<?cs /if ?>  ></td>
		
		<td class="contentsettingscell">TCP Network Port</td>
	      </tr>
	    </table>
	  </td>
	</tr>

	<tr>
	  <td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td>
	</tr>

	<tr>
	  <td class="titlesettingscell" align="right">Destination Keyer:</td>
	  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	  <td class="contentsettingscell" align="left">
	    <table>
	      <?cs set:tmp.has_keyers = 0 ?>
	      <?cs each:item = mhuxd.run.keyer ?>
	      <?cs if:item.info.type != 0 ?>
	      <?cs set:tmp.has_keyers = 1 ?>
	      <tr>
		<td>
		  <input 
		     type="radio" name="set.mhuxd.connector.0.serial" 
		     value="<?cs var:name(item) ?>"
		     <?cs if:(first(item) && !mhuxd.webui.session.set.mhuxd.connector.0.serial)||
			  name(item) == mhuxd.webui.session.set.mhuxd.connector.0.serial ?> checked
		     <?cs /if ?>
		     > <td class="contentsettingscell"><?cs var:item.info.name ?></td>
		</td>
	      </tr>
	      <?cs /if ?>
	      <?cs /each ?>
	    </table>
	  </td>
	</tr>
	

	<tr>
	  <td><img src="/static/dot.gif" width="225" height="1" alt=""></td>
	  <td><img src="/static/dot.gif" width="1" height="1" alt=""></td>
	  <td><img src="/static/dot.gif" width="225" height="1" alt=""></td>
	</tr>
      </table>
    </td>
  </tr>
</table>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<input name="CreatePort" onclick="button_clicked=this.value;" value="Continue" type="submit" <?cs if:tmp.has_keyers == 0 ?>disabled<?cs /if ?>>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>


<?cs /if ?>


