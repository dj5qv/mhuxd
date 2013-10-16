<?cs include:"ptt_cfg.cs" ?>
<?cs include:"ptt_footsw.cs" ?>

<?cs call:sectionheader("PTT Radio 1", "foobar_help") ?>
&nbsp;&nbsp;<br>

<?cs call:ptt_cfg(mhuxd.webui.session.unit, "r1") ?>

<br>
<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.r2 == 1 ?>
<?cs call:sectionheader("PTT Radio 2", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:ptt_cfg(mhuxd.webui.session.unit, "r2") ?>
<br>
<?cs /if ?>

<?cs call:sectionheader("Footswitch Sequencer", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:ptt_footsw(unit, "footsw") ?>

<br><br>
