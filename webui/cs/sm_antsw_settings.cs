<?cs def:sm_antsw_settings1(unit, chan, keyer_type) ?>


<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>


	    <?cs call:opt_select("PA connector CI-V Function",
		 "set.mhuxd.keyer."+unit+".sm.fixed.civFunc", 
		 mhuxd.webui.options.civ_funcs, 
		 mhuxd.keyer[unit].sm.fixed.civFunc ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("CI-V Baud Rate",
		 "set.mhuxd.keyer."+unit+".sm.fixed.civBaudRate", 
		 mhuxd.webui.options.civ_baud, 
		 mhuxd.keyer[unit].sm.fixed.civBaudRate ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("CI-V Address", 
		 "set.mhuxd.keyer."+unit+".sm.fixed.civAddress",
		 mhuxd.keyer[unit].sm.fixed.civAddress) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>


	    <?cs call:opt_select("D9 Serial Port Function",
		 "set.mhuxd.keyer."+unit+".sm.fixed.extSerFunc", 
		 mhuxd.webui.options.sm_ext_ser_func, 
		 mhuxd.keyer[unit].sm.fixed.extSerFunc ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("D9 Serial Port Baud Rate",
		 "set.mhuxd.keyer."+unit+".sm.fixed.extSerBaudRate", 
		 mhuxd.webui.options.civ_baud,
		 mhuxd.keyer[unit].sm.fixed.extSerBaudRate ) 
	     ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Switch Delay (ms)", 
		 "set.mhuxd.keyer."+unit+".sm.fixed.antSwDelay",
		 mhuxd.keyer[unit].sm.fixed.antSwDelay) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Break before make delay (ms)", 
		 "set.mhuxd.keyer."+unit+".sm.fixed.bbmDelay",
		 mhuxd.keyer[unit].sm.fixed.bbmDelay) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number("Inhibit lead time (ms)", 
		 "set.mhuxd.keyer."+unit+".sm.fixed.inhibitLead",
		 mhuxd.keyer[unit].sm.fixed.inhibitLead) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Use Key In", 
		 "set.mhuxd.keyer."+unit+".sm.fixed.useKeyIn",
		 mhuxd.keyer[unit].sm.fixed.useKeyIn) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Invert Key In", 
		 "set.mhuxd.keyer."+unit+".sm.fixed.invertKeyIn",
		 mhuxd.keyer[unit].sm.fixed.invertKeyIn) ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>


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


<?cs def:sm_antsw_sequencer1(unit, chan, keyer_type) ?>
<table class="sectiontable" cellpadding="3" cellspacing="0" width="30%">
  <tbody>
    <tr>
      <td width="33%" class="titlelistcell">&nbsp;&nbsp;</td>
      <td width="33%" class="titlelistcell">Lead</td>
      <td width="33%" class="titlelistcell">Tail</td>
    </tr>


    <?cs each:item = mhuxd.webui.options[keyer_type].outputs ?>
    <?cs if:mhuxd.keyer[unit].sm.output[name(item)] == 2 || mhuxd.keyer[unit].sm.output[name(item)] == 3 ?>



    <tr class="contentlistrow2">
      <td class="contentlistcellbold"><?cs var:name(item) ?></td>
      <td class="contentlistcell">
	<?cs call:opt_number_basic(
	     "set.mhuxd.keyer."+unit+".sm.fixed.sequencer.lead."+name(item), 
	     mhuxd.keyer[unit].sm.fixed.sequencer.lead[name(item)]) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_number_basic(
	     "set.mhuxd.keyer."+unit+".sm.fixed.sequencer.tail."+name(item), 
	     mhuxd.keyer[unit].sm.fixed.sequencer.tail[name(item)]) 
	     ?>
      </td>
    </tr>


    <?cs /if ?>
    <?cs /each ?>

    <tr>
      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
    </tr>
    
  </tbody>
</table>


<?cs /def ?>


<?cs def:sm_antsw_settings(unit, chan) ?>
<?cs call:sm_antsw_settings1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs /def ?>

<?cs def:sm_antsw_sequencer(unit, chan) ?>
<?cs call:sm_antsw_sequencer1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs /def ?>


<?cs call:sectionheader("Settings", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:sm_antsw_settings(mhuxd.webui.session.unit, "sm_antsw_settings") ?>
<br>
<?cs call:sectionheader("Sequencer", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:sm_antsw_sequencer(mhuxd.webui.session.unit, "sm_antsw_sequencer") ?>

<br>
