

<?cs def:sm_antsw_vr_list1(unit, chan) ?>

<!-- BASE PAGE -->


<?cs call:sectionheader("Antenna Groups List", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="3" cellspacing="0" width="60%">
  <tbody>
    <tr>
      <td class="titlelistcell" width="19">&nbsp;</td>
      <td class="titlelistcell">ID</td>
      <td class="titlelistcell">Label</td>
      <td class="titlelistcell">Name</td>
      <td class="titlelistcell">RX-Only</td>
      <td class="titlelistcell">Antenna</td>
    </tr>

    <?cs each:item = mhuxd.keyer[unit].sm.obj ?>
    <?cs if:item.type == 1 && item.virtual_rotator != 1 ?>

    <!-- main row -->
    <tr class="contentlistrowmain">
      <?cs if:mhuxd.webui.session.Edit[chan] ?>
      <td class="contentlistcell" width="19" align="center">&nbsp;</td>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".id", item.id) ?>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".type", item.type) ?>
      <?cs else ?>
      <td class="radiolistcell2" width="19" align="center">
	<input type="checkbox" name="modify.mhuxd.keyer.<?cs var:unit ?>.sm.obj.<?cs var:name(item) ?>" value="1" > 
      </td>
      <?cs /if ?>

      <td class="contentlistcell" width="1%"><?cs var:name(item) ?></td>

      <td class="contentlistcell" width="30%" ><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".label", 
				       mhuxd.keyer[unit].sm.obj[name(item)].label, 5 ) ?></td>

      <td class="contentlistcell" width="30%" ><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".display", 
				       mhuxd.keyer[unit].sm.obj[name(item)].display, 10 ) ?> </td>

      <td class="contentlistcell" width="10%" align="center">&nbsp;</td>

      <td class="contentlistcell"  width="30%" >
	<input name="AddAnt.<?cs var:chan ?>.<?cs var:item.id ?>" value="Add Antenna" type="submit"
	       <?cs if:subcount(mhuxd.webui.session.AddAnt[chan]) 
		    || mhuxd.webui.session.Edit[chan] 
		    || mhuxd.webui.session.Add[chan]?>
	       disabled="1"
	       <?cs /if ?>
	       >
      </td>
    </tr>

    <!-- list ant area -->
    <?cs each:ref_item = mhuxd.keyer[unit].sm.obj[name(item)].ref ?>
    <tr class="contentlistrow2">
      <?cs if:mhuxd.webui.session.Edit[chan] ?>
      <td class="contentlistcell" width="19" align="center">&nbsp;</td>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".ref."+name(ref_item)+".id", ref_item.id) ?>
      <?cs else ?>
      <td class="radiolistcell2" width="19" align="center">
	<input type="checkbox" name="modify.mhuxd.keyer.<?cs var:unit ?>.sm.obj.<?cs var:name(item) ?>.ref.<?cs var:name(ref_item) ?>" value="1" > 
      </td>
      <?cs /if ?>

      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center"><?cs call:bool_ro("", mhuxd.keyer[unit].sm.obj[ref_item.dest_id].rxonly) ?> </td>

      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".ref."+name(ref_item)+".dest_id",
	     mhuxd.keyer[unit].sm.obj,
	     mhuxd.keyer[unit].sm.obj[name(item)].ref[name(ref_item)].dest_id) ?>
      </td>
    </tr>
    <?cs /each ?>

    <!-- add ant area -->
    <?cs if:mhuxd.webui.session.AddAnt[chan][item.id] ?>
    <tr class="contentlistrow2">
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>

      <td class="contentlistcell">
	<?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".type", 1) ?>
	<?cs call:select(
	     "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".ref.0.dest_id",
	     mhuxd.keyer[unit].sm.obj,
	     "") ?>
      </td>
    </tr>
    <?cs /if ?>

    <?cs /if ?>
    <?cs /each ?>

    <!-- Add a new one -->
    <?cs if:mhuxd.webui.session.Add[chan] ?>
    <tr class="contentlistrow2">
      <td class="contentlistcell" width="19" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell" width="1%" align="center">&nbsp;&nbsp;</td>

      <?cs call:hidden("set.mhuxd.keyer."+unit+".sm.obj.0.type", 1) ?>
      <?cs call:hidden("set.mhuxd.keyer."+unit+".sm.obj.0.virtual_rotator", 0) ?>

      <td class="contentlistcell" width="30%"><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.label", 
				       "", 5 ) ?></td>

      <td class="contentlistcell" width="30%"><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.display", 
				       "", 10 ) ?> </td>

      <td class="contentlistcell" width="10%" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell" width="30%" align="center">&nbsp;&nbsp;</td>
    </tr>
    <?cs /if ?>
    <!-- /Add new one -->

  </tbody>
</table>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>



<?cs /def ?>

<?cs def:sm_antsw_vr_list(unit, chan) ?>
<?cs call:sm_antsw_vr_list1(mhuxd.webui.session.unit, chan) ?>

<?cs if:subcount(mhuxd.webui.session.AddAnt[chan]) ?>
<input name="Modify" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs elseif:mhuxd.webui.session.Add[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs elseif:mhuxd.webui.session.Edit[chan] ?>
<input name="Modify" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Add.<?cs var:chan ?>" value="Add" type="submit">
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<input name="Remove" value="Remove" type="submit">
<?cs /if ?>
<?cs /def ?>

<?cs call:sm_antsw_vr_list(unit, "sm_antsw_vr_list") ?>
