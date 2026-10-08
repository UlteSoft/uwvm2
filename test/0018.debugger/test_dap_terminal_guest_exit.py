#!/usr/bin/env python3
"""Original guest exit truth at the unsequenced transport boundary."""
from pathlib import Path
import importlib.util,subprocess,unittest
from unittest.mock import Mock,patch
spec=importlib.util.spec_from_file_location('terminal_exit_server',Path(__file__).resolve().parents[2]/'tools/debug/secure_server.py')
server=importlib.util.module_from_spec(spec);spec.loader.exec_module(server)
class TerminalGuestExit(unittest.TestCase):
 def test_known_os_exit_and_nonzero_status(self):
  for code in (0,7,-15):
   guest=Mock();guest.poll.return_value=code
   self.assertEqual(server.finished_guest_reply(guest,b'wait'),f'guest exited: {code}\n'.encode())
   guest.wait.assert_not_called()
 def test_actual_original_wait_required_after_eof(self):
  guest=Mock();guest.poll.return_value=None;guest.wait.return_value=7
  self.assertEqual(server.finished_guest_reply(guest,b'status',wait=True),b'guest exited: 7\n');guest.wait.assert_called_once_with(timeout=2)
 def test_live_guest_and_unknown_mutation_never_success(self):
  guest=Mock();guest.poll.return_value=None;guest.wait.side_effect=subprocess.TimeoutExpired('original',2)
  self.assertIsNone(server.finished_guest_reply(guest,b'wait',wait=True))
  guest.reset_mock()
  self.assertIsNone(server.finished_guest_reply(guest,b'set memory 0 0 ff',wait=True));guest.poll.assert_not_called();guest.wait.assert_not_called()
 def run_channel(self,command,vm_result=None,error=None):
  guest=Mock();guest.poll.return_value=None;guest.wait.return_value=0
  channel=Mock();channel.send.return_value=len(command)
  if error is not None:channel.send.side_effect=error
  client=Mock();client.send.side_effect=lambda data,flags:len(data)
  with patch.object(server,'checked_packet',side_effect=[b'capability',command,vm_result,None]):
   result=server.serve_client(client,(1,2,3),1,channel,b'capability',guest)
  return result,guest,client,channel
 def test_pipe_close_reports_original_exit_once(self):
  result,guest,client,channel=self.run_channel(b'wait',error=BrokenPipeError())
  self.assertFalse(result);guest.wait.assert_called_once_with(timeout=2)
  self.assertEqual([v.args[0] for v in client.send.call_args_list],[b'ready\n',b'guest exited: 0\n'])
  self.assertEqual(channel.send.call_count,1)
 def test_eof_reports_original_exit_once(self):
  result,guest,client,channel=self.run_channel(b'status')
  self.assertFalse(result);guest.wait.assert_called_once_with(timeout=2)
  self.assertEqual(client.send.call_args_list[-1].args[0],b'guest exited: 0\n')
 def test_uncertain_mutation_or_timeout_remains_failure(self):
  for command,result,error in ((b'set memory 0 0 ff',None,BrokenPipeError()),(b'wait',server._TIMEOUT,None)):
   with self.assertRaises(server.VMChannelFailure):self.run_channel(command,result,error)
 def test_queued_terminal_wait_after_os_exit(self):
  guest=Mock();guest.poll.side_effect=[None,0,0];client=Mock();client.send.side_effect=lambda data,flags:len(data);channel=Mock()
  with patch.object(server,'checked_packet',side_effect=[b'capability',b'wait',None]):
   self.assertFalse(server.serve_client(client,(1,2,3),1,channel,b'capability',guest))
  channel.send.assert_not_called();guest.wait.assert_not_called()
  self.assertEqual(client.send.call_args_list[-1].args[0],b'guest exited: 0\n')
 def test_status_then_wait_after_original_exit(self):
  guest=Mock();guest.poll.side_effect=[None,0,0,0,0];client=Mock();client.send.side_effect=lambda data,flags:len(data);channel=Mock()
  with patch.object(server,'checked_packet',side_effect=[b'capability',b'status',b'wait',None]):
   self.assertFalse(server.serve_client(client,(1,2,3),1,channel,b'capability',guest))
  channel.send.assert_not_called();guest.wait.assert_not_called()
  self.assertEqual([v.args[0] for v in client.send.call_args_list],[b'ready\n',b'guest exited: 0\n',b'guest exited: 0\n'])
if __name__=='__main__':unittest.main()
