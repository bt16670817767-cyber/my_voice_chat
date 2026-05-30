using UnityEngine;

namespace Managers
{
    public class MenuManager : MonoBehaviour
    {
        public static MenuManager Instance;

        public GameObject ConnectionMenu;
        public GameObject ChatMenu;

        private void Awake()
        {
            Instance = this;
        }

        private void Start()
        {
            // 启动时显示登录界面，隐藏语音界面
            ShowConnectionMenu();
        }

        public void ShowConnectionMenu()
        {
            if (ConnectionMenu != null) ConnectionMenu.SetActive(true);
            if (ChatMenu != null) ChatMenu.SetActive(false);
            var handler = FindObjectOfType<ConnectionHandler>();
            if (handler != null) handler.ClearInputs();
        }

        public void ShowChatMenu()
        {
            if (ConnectionMenu != null) ConnectionMenu.SetActive(false);
            if (ChatMenu != null) ChatMenu.SetActive(true);
        }
    }
}