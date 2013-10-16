
<?cs def:mode_cfg_1(unit, chan, keyer_type) ?>
<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs if:chan == "r1" ?>

	    <?cs if:mhuxd.run.keyer[unit].flags.has.follow_tx_mode ?>	    
	    <?cs call:opt_bool("Keyer Mode Follows RIG", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1FollowTxMode", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1FollowTxMode) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs /if ?>

	    <?cs call:opt_select("Current Mode",
		 "set.mhuxd.keyer."+unit+".param.r1KeyerMode", 
		 mhuxd.webui.options.mode, 
		 mhuxd.keyer[unit].param.r1KeyerMode ) 
	     ?>

	    <?cs /if ?>


	    <?cs if:chan == "r2" ?>

	    <?cs if:mhuxd.run.keyer[unit].flags.has.follow_tx_mode ?>	    
	    <?cs call:opt_bool("Keyer Mode Follows RIG", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2FollowTxMode", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2FollowTxMode) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs /if ?>

	    <?cs call:opt_select("Current Mode",
		 "set.mhuxd.keyer."+unit+".param.r2KeyerMode", 
		 mhuxd.webui.options.mode, 
		 mhuxd.keyer[unit].param.r2KeyerMode ) 
	     ?>

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

<?cs /def ?>



<?cs def:mode_cfg(unit, chan) ?>

<?cs call:mode_cfg_1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs /def ?>



