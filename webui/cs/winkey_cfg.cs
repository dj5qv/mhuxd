<?cs def:winkey_cfg1(unit, chan) ?>


<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

<!--
	    <?cs call:opt_number("Speed WPM", 
		 "set.mhuxd.keyer."+unit+".winkey.speedWpm", 
		 mhuxd.keyer[unit].winkey.speedWpm) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
-->

	    <?cs if:keyer_type == 3 || keyer_type == 5 ?>
	    <?cs call:opt_bool("Paddle Only Side Tone", 
		 "set.mhuxd.keyer."+unit+".param.paddleOnlySideTone", 
		 mhuxd.keyer[unit].param.paddleOnlySideTone) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs /if ?>

	    <?cs call:opt_select("Paddle Mode",
		 "set.mhuxd.keyer."+unit+".winkey.keyMode", 
		 mhuxd.webui.options.wk_keyer_modes, 
		 mhuxd.keyer[unit].winkey.keyMode ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("Ultimatic Mode",
		 "set.mhuxd.keyer."+unit+".winkey.ultimaticMode", 
		 mhuxd.webui.options.wk_ultimatic_modes, 
		 mhuxd.keyer[unit].winkey.ultimaticMode ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("PTT Lead (x 10ms)", 
		 "set.mhuxd.keyer."+unit+".winkey.leadInTime", 
		 mhuxd.keyer[unit].winkey.leadInTime) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("PTT Tail (x 10ms)", 
		 "set.mhuxd.keyer."+unit+".winkey.tailTime", 
		 mhuxd.keyer[unit].winkey.tailTime) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Hang Time", 
		 "set.mhuxd.keyer."+unit+".winkey.hangTime", 
		 mhuxd.keyer[unit].winkey.hangTime) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Swap Paddles", 
		 "set.mhuxd.keyer."+unit+".winkey.paddleSwap", 
		 mhuxd.keyer[unit].winkey.paddleSwap) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Auto Space", 
		 "set.mhuxd.keyer."+unit+".winkey.autoSpace", 
		 mhuxd.keyer[unit].winkey.autoSpace) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("CT Spacing", 
		 "set.mhuxd.keyer."+unit+".winkey.ctSpacing", 
		 mhuxd.keyer[unit].winkey.ctSpacing) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Disable Paddle Watchdog", 
		 "set.mhuxd.keyer."+unit+".winkey.disablePaddleWatchdog", 
		 mhuxd.keyer[unit].winkey.disablePaddleWatchdog) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>


	    <?cs call:opt_number("Speed Pot Min WPM", 
		 "set.mhuxd.keyer."+unit+".winkey.minWpm", 
		 mhuxd.keyer[unit].winkey.minWpm) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Speed Pot Range WPM", 
		 "set.mhuxd.keyer."+unit+".winkey.wpmRange", 
		 mhuxd.keyer[unit].winkey.wpmRange) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Farnsworth Speed WPM", 
		 "set.mhuxd.keyer."+unit+".winkey.farnsWpm", 
		 mhuxd.keyer[unit].winkey.farnsWpm) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("DAH/DIT = 3*(nn/50)", 
		 "set.mhuxd.keyer."+unit+".winkey.ditDahRatio", 
		 mhuxd.keyer[unit].winkey.ditDahRatio) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Weighting", 
		 "set.mhuxd.keyer."+unit+".winkey.weight", 
		 mhuxd.keyer[unit].winkey.weight) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("1st Extenstion", 
		 "set.mhuxd.keyer."+unit+".winkey.1stExtension", 
		 mhuxd.keyer[unit].winkey.1stExtension) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Keying Compensation", 
		 "set.mhuxd.keyer."+unit+".winkey.keyComp", 
		 mhuxd.keyer[unit].winkey.keyComp) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Paddle Setpoint", 
		 "set.mhuxd.keyer."+unit+".winkey.paddleSetpoint", 
		 mhuxd.keyer[unit].winkey.paddleSetpoint) ?>


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


<?cs def:winkey_cfg(unit, chan) ?>
<?cs call:winkey_cfg1(unit, chan) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>

<?cs /def ?>
