
<?cs def:ptt_cfg_1(unit, chan, keyer_type) ?>
<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs call:opt_select("PTT CW", 
		 "metaset.mhuxd.webui.meta.keyer."+unit+"."+chan+".ptt_cw", 
		 mhuxd.webui.options[keyer_type].cw.ptt_opts, 
		 mhuxd.webui.meta.keyer[unit][chan].ptt_cw ) 
		 ?>

	    <?cs if:subcount(mhuxd.webui.options[keyer_type].voice.ptt_opts) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_select("PTT Voice", 
		 "metaset.mhuxd.webui.meta.keyer."+unit+"."+chan+".ptt_voice", 
		 mhuxd.webui.options[keyer_type].voice.ptt_opts, 
		 mhuxd.webui.meta.keyer[unit][chan].ptt_voice ) 
		 ?>
	    <?cs /if ?>

	    <?cs if:subcount(mhuxd.webui.options[keyer_type].digital.ptt_opts) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_select("PTT Digital", 
		 "metaset.mhuxd.webui.meta.keyer."+unit+"."+chan+".ptt_digital", 
		 mhuxd.webui.options[keyer_type].digital.ptt_opts, 
		 mhuxd.webui.meta.keyer[unit][chan].ptt_digital ) 
		 ?>
	    <?cs /if ?>


	    <?cs if:mhuxd.run.keyer[unit].flags.has.lna_pa_ptt ?>
	    <?cs if:chan == "r1" ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("LNA PTT", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1LnaPtt", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1LnaPtt) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("PA PTT", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1PaPtt", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1PaPtt) ?>

	    <?cs /if ?>

	    <?cs if:chan == "r2" ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("LNA PTT", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2LnaPtt", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2LnaPtt) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("PA PTT", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2PaPtt", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2PaPtt) ?>

	    <?cs /if ?>
	    <?cs /if ?>


	    <?cs if:mhuxd.run.keyer[unit].flags.has.lna_pa_ptt_tail ?>
	    <?cs if:chan == "r1" ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_number("PA PTT Tail (x 10ms)", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1PaPttTail", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1PaPttTail) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_number("LNA PTT Tail (x 10ms)", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1LnaPttTail", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1LnaPttTail) ?>

	    <?cs /if ?>

	    <?cs if:chan == "r2" ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_number("PA PTT Tail (x 10ms)", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2PaPttTail", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2PaPttTail) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_number("LNA PTT Tail (x 10ms)", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2LnaPttTail", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2LnaPttTail) ?>

	    <?cs /if ?>
	    <?cs /if ?>

	    <?cs if:chan == "r1" ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_number("PTT Lead (x 10ms)", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1PttDelay", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1PttDelay) ?>
	    <?cs /if ?>
	    <?cs if:chan == "r2" ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_number("PTT Lead (x 10ms)", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2PttDelay", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2PttDelay) ?>
	    <?cs /if ?>

	    <?cs if:mhuxd.run.keyer[unit].flags.has.cw_in_voice ?>
	    <?cs if:chan == "r1" ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("CW in Voice", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1AllowCwInVoice", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1AllowCwInVoice) ?>
	    <?cs /if ?>
	    <?cs if:chan == "r2" ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("CW in Voice", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2AllowCwInVoice", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2AllowCwInVoice) ?>
	    <?cs /if ?>
	    <?cs /if ?>

	    <?cs if:mhuxd.run.keyer[unit].flags.has.soundcard_ptt ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Sound Card PTT", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.useAutoPtt", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.useAutoPtt) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Downstream over Footswitch", 
		 "set.mhuxd.keyer."+unit+".param.downstreamoverFootSw", 
		 mhuxd.keyer[unit].param.downstreamoverFootSw) ?>
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



<?cs def:ptt_cfg(unit, chan) ?>

<?cs call:ptt_cfg_1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs /def ?>



