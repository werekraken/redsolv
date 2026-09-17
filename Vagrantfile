Vagrant.configure("2") do |config|
  config.vm.box = "bento/ubuntu-26.04"
  config.vm.provision "shell", path: "provisioner.sh"
  config.vm.provision "shell", path: "contrib/bcc/provisioner.sh"
end
