
devkit_policydir = $(datadir)/PolicyKit/policy

dist_devkit_policy_DATA = 					\
	org.freedesktop.devicekit.power.policy

@INTLTOOL_POLICY_RULE@

check:
	polkit-policy-file-validate $(dist_devkit_policy_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES = $(dist_devkit_policy_DATA)

EXTRA_DIST = $(dist_devkit_policy_DATA:.policy=.policy.in)
