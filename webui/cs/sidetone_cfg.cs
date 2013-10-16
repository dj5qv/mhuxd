<?cs def:sidetone_cfg(unit, chan, keyer_type) ?>

<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs call:opt_select("Side Tone",
		 "set.mhuxd.keyer."+unit+".param.sideTone", 
		 mhuxd.webui.options.side_tone, 
		 mhuxd.keyer[unit].param.sideTone ) 
	     ?>

	    <?cs if:keyer_type == 3 || keyer_type == 5 ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Paddle Only Side Tone", 
		 "set.mhuxd.keyer."+unit+".param.paddleOnlySideTone", 
		 mhuxd.keyer[unit].param.paddleOnlySideTone) ?>
	    <?cs /if ?>

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
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>


<?cs /def ?>
