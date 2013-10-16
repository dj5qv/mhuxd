


<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].info.type == 5 ?>


<?cs call:sectionheader("Audio Switching", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs include:"audio_mk2.cs" ?>
<?cs call:audio_mk2_cfg(mhuxd.webui.session.unit, "r1") ?>

&nbsp;&nbsp;<br>
<?cs call:sectionheader("Audio Options", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs include:"audio_mk2_opts.cs" ?>
<?cs call:audio_mk2_opts(mhuxd.webui.session.unit, "r1opts") ?>


<?cs elif:mhuxd.run.keyer[mhuxd.webui.session.unit].info.type == 4 ?>


<?cs call:sectionheader("Audio Switching", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs include:"audio_mk1.cs" ?>
<?cs call:audio_mk1_cfg(mhuxd.webui.session.unit, "r1") ?>


<?cs elif:mhuxd.run.keyer[mhuxd.webui.session.unit].info.type == 6 || mhuxd.run.keyer[mhuxd.webui.session.unit].info.type == 7 ?>

<?cs include:"audio_mk2r_r1.cs" ?>
<?cs call:audio_mk2r_r1_cfg(mhuxd.webui.session.unit, "r1") ?>
<br><br>
<?cs include:"audio_mk2r_r2.cs" ?>
<?cs call:audio_mk2r_r2_cfg(mhuxd.webui.session.unit, "r2") ?>


<?cs /if ?>
