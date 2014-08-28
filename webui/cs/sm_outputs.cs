<?cs def:sm_outputs1(unit, chan, keyer_type) ?>


<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>


	    <?cs each:item = mhuxd.webui.options[keyer_type].outputs ?>

	    <tr>
	      <td class="titlesettingscell" align="right"><?cs var:item.display ?>:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell">
		<?cs call:opt_select_basic(
		     "set.mhuxd.keyer."+unit+".sm.output."+name(item),
		     mhuxd.webui.options.sm_output_classes,
		     mhuxd.keyer[unit].sm.output[name(item)])
		     ?>

	      </td>
	    </tr>

	    <tr><td class="dividersettingscell" colspan="4"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

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


<?cs def:sm_outputs(unit, chan) ?>
<?cs call:sm_outputs1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>

<?cs /def ?>

<?cs call:sectionheader("Outputs (not functional)", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:sm_outputs(mhuxd.webui.session.unit, "sm_outputs") ?>
<br>
