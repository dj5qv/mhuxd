
<?cs def:summary_cfg_1(unit, chan, keyer_type) ?>
<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs each:item = mhuxd.keyer[mhuxd.webui.session.unit].param ?>
	    <?cs if:subcount(item) ?>

	    <?cs each:subitem = item ?>

	    <?cs call:opt_number(name(item) + "." + name(subitem), 
		 "set.mhuxd.keyer."+unit+".param." + name(item) + "." + name(subitem),
		 subitem) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs /each ?>


	    <?cs else ?>

	    <?cs call:opt_number(name(item), 
		 "set.mhuxd.keyer."+unit+".param." + name(item), 
		 item) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs /if ?>
	    <?cs /each ?>

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

<?cs /def ?>



<?cs def:summary_cfg(unit, chan) ?>

<?cs call:summary_cfg_1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs /def ?>



