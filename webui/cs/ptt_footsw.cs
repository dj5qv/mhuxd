<?cs def:ptt_footsw(unit, chan) ?>

<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs call:opt_bool("Mute serial CW", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.muteCompCw", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.muteCompCw) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Mute serial FSK", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.muteCompFsk", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.muteCompFsk) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Restore serial PTT and audio", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.restorePtt", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.restorePtt) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Restore serial CW", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.restoreCompCw", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.restoreCompCw) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Restore serial FSK", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.restoreCompFsk", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.restoreCompFsk) ?>

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
