"""Provider configuration regressions; actual target runs use the cgroup supervisor."""
from pathlib import Path
import importlib.util,json,tempfile,unittest
from unittest import mock
script=Path(__file__).resolve().parent.parent/'0017.runtime/run_debug_linux_qemu_components.py'
spec=importlib.util.spec_from_file_location('qemu_component_providers',script)
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
class ProviderMapTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
  self.root=Path(self.temp.name).resolve();self.map=self.root/'map.json'
 def write(self,value):
  self.map.write_text(json.dumps(value));return self.map
 def test_independent_selected_roots(self):
  second=self.root/'second';second.mkdir()
  actual=runner.load_provider_map(self.write({'aarch64':str(self.root),'ppc64':str(second)}),{'aarch64','ppc64'})
  self.assertEqual(actual,{'aarch64':self.root,'ppc64':second})
 def test_alias_resolves_to_actual_directory(self):
  alias=self.root/'alias';alias.symlink_to(self.root,target_is_directory=True)
  self.assertEqual(runner.load_provider_map(self.write({'i686':str(alias)}),{'i686'}),{'i686':self.root})
 def test_duplicate_profile_refused(self):
  self.map.write_text('{"i686":"/","i686":"/"}')
  with self.assertRaisesRegex(ValueError,'duplicate'):runner.load_provider_map(self.map,{'i686'})
 def test_missing_extra_and_unknown_profiles_refused(self):
  for value in ({},{'i686':'/','ppc64':'/'},{'unknown':'/'},[]):
   with self.subTest(value=value),self.assertRaisesRegex(ValueError,'every selected'):runner.load_provider_map(self.write(value),{'i686'})
 def test_relative_empty_and_non_string_roots_refused(self):
  for value in ('relative','',None,False,1,[]):
   with self.subTest(value=value),self.assertRaisesRegex(ValueError,'absolute'):runner.load_provider_map(self.write({'i686':value}),{'i686'})
 def test_missing_directory_refused(self):
  with self.assertRaises(FileNotFoundError):runner.load_provider_map(self.write({'i686':str(self.root/'missing')}),{'i686'})
 def test_regular_file_refused(self):
  with self.assertRaisesRegex(ValueError,'not a directory'):runner.load_provider_map(self.write({'i686':str(self.map)}),{'i686'})
 def test_unknown_or_empty_selected_profiles_refused(self):
  for selected in (set(),{'unknown'}):
   with self.subTest(selected=selected),self.assertRaisesRegex(ValueError,'selected'):runner.load_provider_map(self.write({}),selected)
 def test_oversized_map_refused(self):
  self.map.write_bytes(b' '*65537)
  with self.assertRaisesRegex(ValueError,'64 KiB'):runner.load_provider_map(self.map,{'i686'})
 def test_invalid_configuration_starts_no_child_or_output(self):
  self.write({'i686':'relative'});out=self.root/'out'
  argv=[str(script),'--root',str(self.root),'--out',str(out),'--profiles','i686','--cases','debug_source_language_predicate','--repositories','uwvm2','--provider-map',str(self.map)]
  with mock.patch.object(runner.sys,'argv',argv),mock.patch.object(runner.subprocess,'run') as run:
   with self.assertRaises(ValueError):runner.main()
   run.assert_not_called()
  self.assertFalse(out.exists())
 def test_predicate_is_public_component_case(self):
  self.assertEqual(runner.CASES.count('debug_source_language_predicate'),1)
if __name__=='__main__':unittest.main()
