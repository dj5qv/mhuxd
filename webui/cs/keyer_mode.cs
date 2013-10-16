<?cs include:"mode_cfg.cs" ?>

<?cs call:sectionheader("Keyer Mode Radio 1", "foobar_help") ?>
&nbsp;&nbsp;<br>

<?cs call:mode_cfg(mhuxd.webui.session.unit, "r1") ?>

<br><br>
<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.r2 == 1 ?>
<?cs call:sectionheader("Keyer Mode Radio 2", "foobar_help") ?>
&nbsp;&nbsp;<br>

<?cs call:mode_cfg(mhuxd.webui.session.unit, "r2") ?>

<br><br>
<?cs /if ?>

