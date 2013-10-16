<?cs def:display_cfg1(unit, chan, keyer_type) ?>


<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs call:opt_number("Light (0-25)", 
		 "set.mhuxd.keyer."+unit+".param.pwmBlight", 
		 mhuxd.keyer[unit].param.pwmBlight) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Contrast (0-25)", 
		 "set.mhuxd.keyer."+unit+".param.pwmContrast", 
		 mhuxd.keyer[unit].param.pwmContrast) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("Background Upper Line",
		 "set.mhuxd.keyer."+unit+".param.dispBg0", 
		 mhuxd.webui.options.mk2_dispbk, 
		 mhuxd.keyer[unit].param.dispBg0 ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("Background Lower Line",
		 "set.mhuxd.keyer."+unit+".param.dispBg1", 
		 mhuxd.webui.options.mk2_dispbk, 
		 mhuxd.keyer[unit].param.dispBg1 ) 
		 ?>

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


<?cs def:display_cfg(unit, chan) ?>
<?cs call:display_cfg1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>

<?cs /def ?>
